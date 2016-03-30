// server.cpp
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"
#include "icpReceivedData.h"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <thread>
#include <iostream>
#include <queue>
#include <utility>
#include <boost/array.hpp>
#include <time.h>
#include "fftw3.h"

//#include <cstdlib>
//#include <boost/bind.hpp>
//#include <boost/smart_ptr.hpp>
//#include <boost/thread/thread.hpp>

using boost::asio::ip::tcp;

const unsigned char my_version   =  2;

// Dataframe size is of type uint64_t
const size_t   DATAFRAME_SIZE_FIELD_SIZE  =  sizeof (uint64_t);

// Entity Header size with 0 padding is sizeof (uint32_t) times 4
const size_t   ENTITY_HEADER_SIZE         = sizeof (uint32_t) * 4;

// TODO: const unsigned char stamp[4] = {'I', 'M', 'R', 'D'};

const int __one__ = 1;
const bool isCpuLittleEndian = 1 == *(char*)(&__one__);


typedef std::shared_ptr<tcp::socket> socket_ptr;

struct INPUT_MESSAGE_STRUCTURE
{
  uint64_t                   size;
  ISMRMRD::EntityHeader      ehdr;
  uint32_t                   data_size;
  std::vector<unsigned char> data;
};

struct OUTPUT_MESSAGE_STRUCTURE
{
  uint32_t                   size;
  std::vector<unsigned char> data;
};

typedef std::shared_ptr<INPUT_MESSAGE_STRUCTURE>  IN_MSG;
typedef std::shared_ptr<std::queue<IN_MSG> >      INPUT_QUEUE;

typedef std::shared_ptr<OUTPUT_MESSAGE_STRUCTURE> OUT_MSG;
typedef std::shared_ptr<std::queue<OUT_MSG> >     OUTPUT_QUEUE;

//Helper function for the FFTW library
template <typename T>
void circshift(T *out, const T *in, int xdim, int ydim, int xshift, int yshift)
{
  for (int i =0; i < ydim; i++)
  {
    int ii = (i + yshift) % ydim;
    for (int j = 0; j < xdim; j++)
    {
      int jj = (j + xshift) % xdim;
      out[ii * xdim + jj] = in[i * xdim + j];
    }
  }
}

#define fftshift(out, in, x, y) circshift(out, in, x, y, (x/2), (y/2))

/*******************************************************************************
 ******************************************************************************/
void writeSocket (socket_ptr sock, OUTPUT_QUEUE oq, int id)
{
  std::cout << "Writer thread (" << id << ") started\n" << std::flush;

  OUT_MSG msg;
  bool done = false; // TODO: determine exit criteria
  try
  {
    while (!done)
    {
      if ((*oq).size() <= 0)
      {
        sleep (1);
        continue;
      }
      
      msg = (*oq).front();
      (*oq).pop();
      printf ("About to write, message size = %u\n", (*msg).size);

      boost::asio::write (*sock, boost::asio::buffer ((*msg).data, (*msg).size));
    }

    std::cout << "Writer (" << id << "): Done!\n";
  }
  catch (std::exception& e)
  {
    std::cerr << "Writer (" << id << ") Exception: " << e.what() << "\n";
  }
}

/*******************************************************************************
 ******************************************************************************/
void queueMessage
(
  uint32_t                    size,
  std::vector<unsigned char>& ent,
  std::vector<unsigned char>& data,
  OUTPUT_QUEUE                oq
)
{
  OUT_MSG msg;
  (*msg).size = size;
  (*msg).data.reserve (size);

  std::cout << "Queueing message of size = " << size << std::endl;
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));
  printf ("data size = %ld, sizeof = %ld\n", data.size(), sizeof (data));

  uint64_t s = size;
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  std::cout << "Size = " << size << std::endl;

  std::copy ((unsigned char*) &s,
             (unsigned char*) &s + sizeof (s),
             std::back_inserter ((*msg).data));

  std::copy ((unsigned char*) &ent,
             (unsigned char*) &ent + ent.size(),
             std::back_inserter ((*msg).data));

  std::copy ((unsigned char*) &data,
             (unsigned char*) &data + data.size(),
             std::back_inserter ((*msg).data));

  (*oq).push (msg);
}


/*******************************************************************************
 ******************************************************************************/
void queueHandshakeMsg
(
  OUTPUT_QUEUE              oq,
  ISMRMRD::ConnectionStatus status,
  char*                     client_name,
  uint64_t                  timestamp = 0
)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);

  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65536;
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  ISMRMRD::Handshake handshake;
  handshake.timestamp = timestamp;
  handshake.conn_status = (uint32_t) status;
  strncpy (client_name, handshake.client_name, strlen (client_name));
  std::vector<unsigned char> hand = handshake.serialize();
  printf ("hand size = %ld, sizeof = %ld\n", hand.size(), sizeof (hand));

  queueMessage (ent.size() + hand.size(), ent, hand, oq);
}

/*******************************************************************************
 ******************************************************************************/
void queueCommandMsg
(
  OUTPUT_QUEUE          oq,
  ISMRMRD::CommandType  cmd_type = ISMRMRD::ISMRMRD_COMMAND_NO_COMMAND
)
{
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65537; // Todo: define reserved stream number names
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  ISMRMRD::Command   cmd;
  cmd.command_type = cmd_type;
  cmd.command_id   = 0;
  cmd.num_streams  = 0;
  cmd.config_size  = 0;
  std::vector<unsigned char> command = cmd.serialize();
  printf ("command size = %ld, sizeof = %ld\n", command.size(), sizeof (command));

  queueMessage (ent.size() + command.size(), ent, command, oq);
}

/*******************************************************************************
 ******************************************************************************/
void rejectConnection
(
  OUTPUT_QUEUE               oq,
  uint64_t                   timestamp,
  ISMRMRD::ConnectionStatus  status,
  char*                      client_name
)
{
  queueHandshakeMsg (oq,
                       status,
                       client_name,
                       timestamp);

  queueCommandMsg (oq,
                     ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER);
}

/*******************************************************************************
 ******************************************************************************/
int receiveFrameInfo (socket_ptr sock, IN_MSG in_msg)
{
  boost::system::error_code  error;
  std::vector<unsigned char> buffer;

  buffer.reserve (DATAFRAME_SIZE_FIELD_SIZE);
  boost::asio::read (*sock,
                     boost::asio::buffer (&buffer[0], DATAFRAME_SIZE_FIELD_SIZE),
                     error);

  std::copy (&buffer[0],
             &buffer[DATAFRAME_SIZE_FIELD_SIZE],
             (unsigned char*)&(*in_msg).size);

  std::cout << "frame_size read status: " << error << std::endl;
  std::cout << "frame_size            : " << (*in_msg).size << std::endl;

  buffer.resize (ENTITY_HEADER_SIZE);
  boost::asio::read (*sock,
                     boost::asio::buffer (&buffer[0], ENTITY_HEADER_SIZE),
                     error);

  std::cout << "EntityHeader read status: " << error << std::endl;
  (*in_msg).ehdr.deserialize (buffer);

  (*in_msg).data_size = (*in_msg).size - ENTITY_HEADER_SIZE;

  std::cout << "sizeof (EntityHeader)   : " << sizeof (ISMRMRD::EntityHeader) <<
            std::endl;
  std::cout << "version                 : " <<
               std::to_string ((*in_msg).ehdr.version) << std::endl;
  std::cout << "entity_type             : " <<
               (ISMRMRD::EntityType) (*in_msg).ehdr.entity_type <<
               std::endl;
  std::cout << "storage_type            : " <<
               (ISMRMRD::StorageType) (*in_msg).ehdr.storage_type <<
               std::endl;
  std::cout << "stream                  : " <<
               (*in_msg).ehdr.stream << std::endl;
  std::cout << "data size               : " <<
               (*in_msg).data_size << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void receiveHandshake (socket_ptr            sock,
                        ISMRMRD::Handshake&   handshake)
{
  boost::system::error_code  error;
  IN_MSG in_msg;

  receiveFrameInfo (sock, in_msg);

  if ((*in_msg).ehdr.entity_type != ISMRMRD::ISMRMRD_HANDSHAKE)
  {
    // TODO: throw an error here, probably need to shutdown
    std::cout << "Error! Expected Handshake, received " << 
                 (*in_msg).ehdr.entity_type << std::endl;
  }

  boost::asio::read (*sock,
                     boost::asio::buffer (&(*in_msg).data[0],
                                          (*in_msg).data_size),
                     error);

  handshake.deserialize (std::vector<unsigned char> ((*in_msg).data.begin(),
                                                     (*in_msg).data.end()));

  std::cout << "Handshake read status: " << error << std::endl;
  std::cout << "Handshake struct size: " << sizeof (ISMRMRD::Handshake) <<
               std::endl;
  std::cout << "Timestamp            :" << handshake.timestamp << std::endl;
  std::cout << "Client name          :" << handshake.client_name << std::endl;
}

/*******************************************************************************
 ******************************************************************************/
void receiveMessage (socket_ptr sock, IN_MSG in_msg)
{
  boost::system::error_code  error;
  receiveFrameInfo (sock, in_msg);

  if ((*in_msg).data_size <= 0)
  {
    // TODO: Throw an error (unless we can have an entity_header as a command ??)
    std::cout << "Error! The data_size = " << (*in_msg).data_size << std::endl;
    std::cout << "EntityType = " << (*in_msg).ehdr.entity_type << std::endl;
    return;
  }

  (*in_msg).data.reserve ((*in_msg).data_size);
  boost::asio::read (*sock,
                     boost::asio::buffer (&(*in_msg).data[0],
                                          (*in_msg).data_size),
                     error);

  // TODO: Check read status and throw an error if bad
  std::cout << "Read status : " << error << std::endl;
  std::cout << "Data size   : " << (*in_msg).data_size << std::endl;
  std::cout << "Entity Type : " <<
               (*in_msg).ehdr.entity_type << std::endl;
}

/*******************************************************************************
 ******************************************************************************/
ISMRMRD::ConnectionStatus authenticateClient (ISMRMRD::Handshake& handshake)
{
  ISMRMRD::ConnectionStatus status = ISMRMRD::CONNECTION_ACCEPTED;

  /*
  if (num_connections > max_num_connections)
  {
    status = ISMRMRD::CONNECTION_DENIED_SERVER_BUSY;
  }
  else if (!clients.find (handshake.client_name))
  {
    status = ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER;
  }
  */

  return status;
}

/*******************************************************************************
 ******************************************************************************/
void queueXMLHeaderMsg
(
  std::vector<unsigned char> data,
  OUTPUT_QUEUE               oq
)
{
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_XML_HEADER;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 1; // Todo: define reserved stream number names
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  queueMessage (ent.size() + data.size(), ent, data, oq);
}

/*******************************************************************************
 ******************************************************************************/
void processImageReconRequest (ICPRECEIVEDDATA::ReceivedData& in_data,
                               ISMRMRD::Command&              command,
                               OUTPUT_QUEUE                   oq)
{

  ISMRMRD::IsmrmrdHeader xml_hdr = in_data.getXMLHeader();
  // There is nothing to change in the xml header for this demo but send it back
  std::stringstream str;
  ISMRMRD::serialize (xml_hdr, str);

  std::vector<unsigned char> xml_str;

  queueXMLHeaderMsg ((*in_msg).data, out_mq);

  ISMRMRD::IsmrmrdHeader hdr;
  ISMRMRD::deserialize((*in_msg).data, hdr); // needs to be redone

  ISMRMRD::EncodingSpace e_space = hdr.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = hdr.encoding[0].reconSpace;
  uint16_t eX = e_space.matrixSize.x;
  uint16_t eY = e_space.matrixSize.y;
  //uint16_t eZ = e_space.matrixSize.z;
  uint16_t rX = r_space.matrixSize.x;
  uint16_t rY = r_space.matrixSize.y;
  uint16_t rZ = r_space.matrixSize.z;
  
  // Get first acquisition to read the number of coils
  in_msg = (*in_msg).front(); // Fix
  (*in_msg).pop();

  ISMRMRD::EntityType e_type =
    (ISMRMRD::EntityType)((*in_msg).ehdr.entity_type);
  ISMRMRD::StorageType e_type =
    (ISMRMRD::StorageType)((*in_msg).ehdr.entity_type);
  uint32_t stream = (*in_msg).ehdr.stream;



  ISMRMRD::Acquisition<float> acq; // Look up the data type in the entity header
  // Need to group the acquisitions by their stream numbers. Every stream will be
  // reconstructed separately
  acq.deserialize (std::vector<unsigned char>(msg.data.begin(), msg.data.end()));
  uint16_t num_coils = acq.getActiveChannels();

  std::vector<size_t> dims;
  dims.push_back (eX);
  dims.push_back (eY);
  dims.push_back (num_coils);
  ISMRMRD::NDArray<std::complex<float> >buffer (dims);

  for (uint16_t ii = 0;  msg.ehdr.entity_type == ISMRMRD::ISMRMRD_MRACQUISITION; ++ii)
  {
    for (uint16_t coil = 0; coil < num_coils; coil++)
    {
      memcpy (&buffer.at(0, acq.getHead().idx.kspace_encode_step_1, coil),
        &acq.at(0, coil), sizeof (std::complex<float>) * eX);
    }
    msg = (*in_msg).front();
    acq.deserialize (std::vector<unsigned char>(msg.data.begin(), msg.data.end()));
    (*in_msg).pop();
  }

  for (uint16_t coil = 0; coil < num_coils; coil++)
  {
    fftwf_complex* tmp = (fftwf_complex*)fftwf_malloc (sizeof (fftwf_complex) * (eX * eY));
    if (!tmp)
    {
      std::cout << "Error allocating temporary storage for FFTW" << std::endl;
      queueErrorCommand (out_msg, ISMRMRD::ISMRMRD_ERROR_INTERNAL_ERROR);
      return;
    }

    //Create the FFTW plan
    fftwf_plan p = fftwf_plan_dft_2d (eY, eX, tmp, tmp, FFTW_BACKWARD, FFTW_ESTIMATE);

    //FFTSHIFT
    fftshift (reinterpret_cast<std::complex<float>*> (tmp), &buffer.at (0, 0, coil), eX, eY);

    //Execute the FFT
    fftwf_execute (p);

    //FFTSHIFT
    fftshift (&buffer.at (0, 0, coil), reinterpret_cast<std::complex<float>*> (tmp), eX, eY);

    //Clean up.
    fftwf_destroy_plan (p);
    fftwf_free (tmp);
  }

  //Allocate an image
  ISMRMRD::Image<float> img_out (rX, rY, rZ, 1);
  //memset (img_out.getDataPtr(), 0, sizeof (float_t) * rX * rY);

  //f there is oversampling in the readout direction remove it
  //Take the sqrt of the sum of squares
  uint16_t offset = ((e_space.matrixSize.x - r_space.matrixSize.x) >> 1);
  for (uint16_t y = 0; y < rY; y++)
  {
    for (uint16_t x = 0; x < rX; x++)
    {
      for (uint16_t coil = 0; coil < num_coils; coil++)
      {
        img_out.at (x, y) += (std::abs (buffer.at (x + offset, y, coil))) *
                            (std::abs (buffer.at (x + offset, y, coil)));
      }
      img_out.at (x, y) = std::sqrt (img_out.at (x, y));
    }
  }

  // The following are extra guidance we can put in the image header
  img_out.setImageType (ISMRMRD::ISMRMRD_IMTYPE_MAGNITUDE);
  img_out.setSlice (0);
  img_out.setFieldOfView (r_space.fieldOfView_mm.x,
                          r_space.fieldOfView_mm.y,
                          r_space.fieldOfView_mm.z);
  //And so on

  //Let's write the reconstructed image into the same data file
  msg.size = sizeof (ISMRMRD::EntityHeader) + sizeof (float_t) * rX * rY * rZ;
  msg.ehdr.version = my_version;
  msg.ehdr.entity_type = ISMRMRD::ISMRMRD_IMAGE;
  msg.ehdr.storage_type = ISMRMRD::ISMRMRD_FLOAT;
  msg.ehdr.stream = 65536;
  msg.data = (char*)&(img_out.serialize())[0];
  (*out_msg).push (msg);

  msg.size = sizeof (ISMRMRD::EntityHeader) + sizeof (ISMRMRD::Command);
  msg.ehdr.version = my_version;
  msg.ehdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
  msg.ehdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  msg.ehdr.stream = 65537;
  cmd.command_type = ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER;
  cmd.error_type   = ISMRMRD::ISMRMRD_ERROR_NO_ERROR;
  msg.data = (char*)&(cmd.serialize())[0];
  (*out_msg).push (msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
bool authenticateClient
(
  socket_ptr          sock,
  OUTPUT_QUEUE        out_mq,
  ISMRMRD::Handshake& handshake
)
{
  bool accepted = false;

  // Receive and authenticate the Handshake
  receiveHandshake (sock, handshake);

  ISMRMRD::ConnectionStatus conn_status;

  printf ("Autenticating client\n");

  if ((conn_status = authenticateClient (handshake)) !=
       ISMRMRD::CONNECTION_ACCEPTED)
  {
    printf ("Rejecting client\n");
    rejectConnection (out_mq,
                       handshake.timestamp,
                       conn_status,
                       handshake.client_name);
  }
  else
  {
    printf ("About to queue handshake response\n");
    queueHandshakeMsg (out_mq, ISMRMRD::CONNECTION_ACCEPTED,
                       handshake.client_name,
                       handshake.timestamp);
    accepted = true;
  }

  return accepted;
}

/*******************************************************************************
 ******************************************************************************/
void processRemainingData
(
  ICPRECEIVEDDATA::ReceivedData in_data,
  OUTPUT_QUEUE                  out_mq,
  std::vector<std::thread>&     thread_v
)
{
  // TODO:

  // We are making an assumption here that any input data that was previously
  // processed was then either deleted from the input data, or marked as
  // processed.

  // We will now look for any remaining input data that was not associated
  // with any of the received commands and decide what should be done with it.
  // For every processing action we decide to take we will spawn a new thread.
  // If after all processing there is still some data that we don't know what
  // to do with - report an error to the respondent and discard the data.
  // And then return.

  // TODO:
  // Should consider calling this routine periodically so it may determine if
  // there already is some input data not associated with any commands, that we
  // can start processing immediatelly.

  return;
}

/*******************************************************************************
 ******************************************************************************/
void processCommand
(
  ISMRMRD::Command              cmd,
  ICPRECEIVEDDATA::ReceivedData in_data,
  OUTPUT_QUEUE                  out_mq,
  std::vector<std::thread>&     thread_v
)
{
  switch (cmd.command_type)
  {
    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      in_data.setRespondentDone();
      std::cout << "Received the STOP command from client" << std::endl;
      // Process data not associated with any of the received commands
      processRemainingData (in_data, out_mq, thread_v);
      break;

    case ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION:

      thread_v.push_back (std::thread (processImageReconRequest,
                                       std::ref (in_data),
                                       std::ref (cmd),
                                       out_mq));
      break;

    default:

      // throw an error
      std::cout << "No matching processing task for the command " <<
                   cmd.command_type << std::endl;
      break;
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void readSocket
(
  socket_ptr sock,
  int        id
)
{
  // Start the writer thread
  OUTPUT_QUEUE        out_mq;
  std::thread writer (writeSocket, sock, out_mq, id);

  ISMRMRD::Handshake handshake;
  if (!authenticateClient (sock, out_mq, handshake))
  {
    writer.join();
    return;
  }

  ICPRECEIVEDDATA::ReceivedData in_data (handshake.client_name,
                                         handshake.timestamp);
  std::vector<std::thread> thread_v;
  // Keep reading messages until respondents STOP/DONE command is received
  while (!in_data.isRespondentDone())
  {
    IN_MSG in_msg;
    receiveMessage (sock, in_msg);
    ISMRMRD::Command cmd;

    switch ((*in_msg).ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize ((*in_msg).data);
        processCommand (cmd, in_data, out_mq, thread_v);
        break;

      case ISMRMRD::ISMRMRD_ERROR:

        // TODO: Error specific processing here...
        // Consider adding to the Received data
        break;

      case ISMRMRD::ISMRMRD_XML_HEADER:

        // TODO: XML Header specific processing here...
        in_data.addXMLHeader ((*in_msg).ehdr, (*in_msg).data);
        break;

      case ISMRMRD::ISMRMRD_MRACQUISITION:
      case ISMRMRD::ISMRMRD_WAVEFORM:
      case ISMRMRD::ISMRMRD_IMAGE:
      case ISMRMRD::ISMRMRD_BLOB:

        in_data.addToStream ((*in_msg).ehdr, (*in_msg).data);
        break;

      default:

        // TODO: Throw Unexpected entity type error.
        std::cout << "Warning! unexpected Entity Type received: " <<
                     (*in_msg).ehdr.entity_type << std::endl;
        break;

    } // switch ((*in_msg).ehdr.entity_type)
  } // while (!in_data.isRespondentDone())

  printf ("Reader is done, waiting for processing threads to join\n");
		for (int ii = 0; ii < thread_v.size(); ii++)
  {
    thread_v[ii].join();
  }
  printf ("Processors joined, now waiting for writer to join\n");
  writer.join();
  printf ("Writer joined, reader exiting\n");

  return;
}

/*******************************************************************************
 ******************************************************************************/
int main
(
  int   argc,
  char* argv[]
)
{
  int port;

  try
  {
    if (argc != 2) port = 50050;
    else           port = std::atoi (argv[1]);

    std::cout << "Using port number " << port << std::endl;
    std::cout << "To connect to a different port, restart: iserv <port>\n\n";

    boost::asio::io_service io_service;
    tcp::acceptor a (io_service, tcp::endpoint (tcp::v4(), port));

    for (int id = 1;; ++id)
    {
      socket_ptr sock (new tcp::socket (io_service));
      a.accept (*sock);
      std::cout << "Accepted client (id " << id <<
                   "), starting communications...\n";
      std::thread (readSocket, sock, id).detach();
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "Main thread Exception: " << e.what() << "\n";
  }

  return 0;
}
