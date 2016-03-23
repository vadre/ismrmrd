// server.cpp
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"
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
const size_t   DATAFRAME_SIZE_FIELD_SIZE  =  8;

// Entity Header size with 0 padding is sizeof (uint32_t) times 4
const size_t   ENTITY_HEADER_SIZE         = 16;

const size_t   MIN_MSG_SIZE = DATAFRAME_SIZE_FIELD_SIZE + ENTITY_HEADER_SIZE;

// TODO: const unsigned char stamp[4] = {'I', 'M', 'R', 'D'};



typedef std::shared_ptr<tcp::socket> socket_ptr;

struct MESSAGE_STRUCTURE
{
  uint64_t                   size;
  ISMRMRD::EntityHeader      ehdr;
  uint32_t                   data_size;
  std::vector<unsigned char> data;
};

typedef std::shared_ptr<MESSAGE_STRUCTURE>           MSG_STRUCT;
typedef std::shared_ptr<std::queue<MSG_STRUCT> >     MSG_STRUCT_QUEUE;

typedef std::shared_ptr<std::vector<unsigned char> > MESSAGE;
typedef std::shared_ptr<std::queue<MESSAGE> >        MSG_QUEUE;

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
void write_socket (socket_ptr sock, MSG_QUEUE mq, int id)
{
  std::cout << "Writer thread (" << id << ") started\n" << std::flush;

  MESSAGE_STRUCT msg;
  bool done = false;
  try
  {
    while (!done)
    {
      if (!done && msgs->size() <= 0)
      {
        sleep (1);
        continue;
      }
      
      printf ("About to write, message size = %ld\n", msgs->size());
      msg = (*msgs).front();
      (*msgs).pop();
      std::cout << "msg.size = " << msg.size << std::endl;
      std::cout << "msg.ehdr.entity_type = " << msg.ehdr.entity_type << std::endl;
      std::cout << "msg.size = " << msg.size << std::endl;
      std::string data = std::to_string (msg.size);
      data.append ((char*) &(msg.ehdr.serialize())[0], sizeof (ISMRMRD::EntityHeader));
      data.append (msg.data);

      if (msg.ehdr.entity_type == ISMRMRD::ISMRMRD_COMMAND)
      {
        ISMRMRD::Command cmd;
        memcpy ((void*)&cmd, (void*)&msg.data, sizeof (ISMRMRD::ISMRMRD_COMMAND));
        if (cmd.command_type == ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER)
        {
          done = true;
          std::cout << "Writer (" << id << "): Sending Stop command" << std::endl;
        }
      }

      boost::asio::write (*sock, boost::asio::buffer (data, data.size()));
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
void reject_connection
(
  MSG_QUEUE                  mq,
  uint64_t                   timestamp,
  ISMRMRD::ConnectionStatus  status,
  char*                      client_name
)
{
  queue_handshake_msg (mq,
                       timestamp,
                       status,
                       client_name);

  queue_command_msg (mq,
                     ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER,
                     ISMRMRD::ISMRMRD_ERROR_INPUT_DATA_ERROR);
}

/*******************************************************************************
 ******************************************************************************/
void queue_message
(
  uint64_t& size,
  std::vector<unsigned char>& ent,
  std::vector<unsigned char>& data,
  MSG_QUEUE mq
)
{
  MESSAGE msg;
  (*msg).reserve (size + sizeof (size));

  std::cout << "Size = " << size << std::endl;
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));
  printf ("data size = %ld, sizeof = %ld\n", data.size(), sizeof (data));

  std::copy ((unsigned char*) &size,
             (unsigned char*) &size + sizeof (size),
             std::back_inserter (*msg));

  std::copy ((unsigned char*) &ent,
             (unsigned char*) &ent + ent.size(),
             std::back_inserter (*msg));

  std::copy ((unsigned char*) &data,
             (unsigned char*) &data + data.size(),
             std::back_inserter (*msg));

  (*mq).push (msg);
}


/*******************************************************************************
 ******************************************************************************/
void queue_handshake_msg
(
  MSG_QUEUE                 mq,
  uint64_t                  timestamp = 0,
  ISMRMRD::ConnectionStatus status,
  char*                     client_name
)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65536;
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  handshake.timestamp = timestamp;
  handshake.conn_status = (uint32_t) status;
  strncpy (client_name, andshake.client_name, strlen (client_name));
  std::vector<unsigned char> hand = handshake.serialize();
  printf ("hand size = %ld, sizeof = %ld\n", hand.size(), sizeof (hand));

  uint64_t  size = (uint64_t) (ent.size() + hand.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  std::cout << "Size = " << size << std::endl;

  queue_message (size, ent, hand, mq);
}

/*******************************************************************************
 ******************************************************************************/
int receive_frame_info (socket_ptr sock, MESSAGE_STRUCT msg_struct)
{
  boost::system::error_code  error;
  std::vector<unsigned char> buffer;

  buffer.reserve (DATAFRAME_SIZE_FIELD_SIZE);
  boost::asio::read (*sock,
                     boost::asio::buffer (&buffer[0], DATAFRAME_SIZE_FIELD_SIZE),
                     error);

  std::copy (&buffer[0],
             &buffer[DATAFRAME_SIZE_FIELD_SIZE],
             (unsigned char*)&(*msg_struct).size);

  std::cout << "frame_size read status: " << error << std::endl;
  std::cout << "frame_size            : " << (*msg_struct).size << std::endl;

  buffer.resize (ENTITY_HEADER_SIZE);
  boost::asio::read (*sock,
                     boost::asio::buffer (&buffer[0], ENTITY_HEADER_SIZE),
                     error);
  (*msg_struct).ehdr.deserialize (buffer);

  (*msg_struct).data_size = (*msg_struct).size - ENTITY_HEADER_SIZE;

  std::cout << "EntityHeader read status: " << error << std::endl;
  std::cout << "sizeof (EntityHeader)   : " << sizeof (ISMRMRD::EntityHeader) <<
            std::endl;
  std::cout << "version                 : " <<
               std::to_string ((*msg_struct).ehdr.version) << std::endl;
  std::cout << "entity_type             : " <<
               (ISMRMRD::EntityType) (*msg_struct).ehdr.entity_type <<
               std::endl;
  std::cout << "storage_type            : " <<
               (ISMRMRD::StorageType) (*msg_struct).ehdr.storage_type <<
               std::endl;
  std::cout << "stream                  : " <<
               (*msg_struct).ehdr.stream << std::endl;
  std::cout << "data size               : " <<
               (*msg_struct).data_size << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void receive_handshake (socket_ptr            sock,
                        ISMRMRD::Handshake&   handshake)
{
  boost::system::error_code  error;
  MESSAGE_STRUCT msg_struct;

  receive_frame_info (sock, msg_struct);

  if ((*msg_struct).ehdr.entityType != ISMRMRD::Handshake)
  {
    // TODO: throw an error here, probably need to shutdown
    std::cout << "Error! Expected Handshake, received " << 
                 (*msg_struct).ehdr.entityType << std::endl;
  }

  boost::asio::read (*sock,
                     boost::asio::buffer (&msg_struct.data[0],
                                          sizeof (ISMRMRD::Handshake)),
                     error);

  handshake.deserialize (std::vector<unsigned char> (msg.data.begin(),
                                                     msg.data.end()));

  std::cout << "Handshake read status: " << error << std::endl;
  std::cout << "Handshake struct size: " << sizeof (ISMRMRD::Handshake) <<
               std::endl;
  std::cout << "Timestamp            :" << handshake.timestamp << std::endl;
  std::cout << "Client name          :" << handshake.client_name << std::endl;
}

/*******************************************************************************
 ******************************************************************************/
void receive_message (socket_ptr sock, MESSAGE_STRUCT msg_struct)
{
  boost::system::error_code  error;
  receive_frame_info (sock, msg_struct);

  if ((*msg_struct).data_size <= 0)
  {
    // TODO: Throw an error (unless we can have an entity_header as a command ??)
    std::cout << "Error! The data_size = " << (*msg_struct).data_size << std::endl;
    std::cout << "EntityType = " << (*msg_struct).ehdr.entity_type << std::endl;
    return;
  }

  (*msg_struct).data.reserve ((*msg_struct).data_size);
  boost::asio::read (*sock,
                     boost::asio::buffer (&(*msg_struct).data[0],
                                          (*msg_struct).data_size),
                     error);

  // TODO: Check read status and throw an error if bad
  std::cout << "Read status : " << error << std::endl;
  std::cout << "Data size   : " << (*msg_struct).data_size << std::endl;
  std::cout << "Entity Type : " <<
               (*msg_struct).ehdr.entity_type << std::endl;
}

/*******************************************************************************
 ******************************************************************************/
ISMRMRD::ConnectionStatus authenticate_client (ISMRMRD::Handshake& handshake)
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
void receive_image_recon_data
(
  socket_ptr             sock,
  MSG_STRUCT_QUEUE&             in_msg
)
{
  boost::system::error_code  error;
  ISMRMRD::Command           cmd;

  do
  {
    MESSAGE_STRUCT msg;
    receive_frame_info (sock, msg);
    boost::asio::read (*sock,
                       boost::asio::buffer (&msg.data[0],
                                            msg.size -
                                              sizeof (ISMRMRD::EntityHeader)),
                       error);
    (*in_msg).push (msg);

    if (msg.ehdr.entity_type == ISMRMRD::ISMRMRD_COMMAND)
    {
      cmd.deserialize (std::vector<unsigned char> (msg.data.begin(), msg.data.end()));
    }

  } while (cmd.command_type != ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void queue_command_msg
(
  MSG_QUEUE             mq,
  ISMRMRD::ErrorType    err_type = ISMRMRD::ISMRMRD_ERROR_NO_ERROR,
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
  cmd.error_type   = err_type;
  std::vector<unsigned char> command = cmd.serialize();
  printf ("command size = %ld, sizeof = %ld\n", command.size(), sizeof (command));

  uint64_t size = (uint64_t) (ent.size() + command.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queue_message (size, ent, command, mq);
}

/*******************************************************************************
 ******************************************************************************/
void queue_xml_header_msg
(
  std::vector<unsigned char> data,
  MSG_QUEUE                  mq
)
{
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_XML_HEADER;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 1; // Todo: define reserved stream number names
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  uint64_t size = (uint64_t) (ent.size() + data.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queue_message (size, ent, data, mq);
}

/*******************************************************************************
 ******************************************************************************/
void process_received_data
(
  ICPRECEIVEDDATA::ReceivedData& in_data,
  MSG_QUEUE                      out_mq
)
{
  bool                 not_done = true;
  ISMRMRD::CommandType cmd;
  uint32_t             stream;
  while (in_data.getCommand (cmd, stream))
  {
    switch (cmd)
    {
      case ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION:
        process_image_recon_request (in_data, stream, out_mq);
        break;
      default:
        // throw an error
        std::cout << "No match for the command " << cmd << std::endl;
        break;
    }
  }
}
/*******************************************************************************
 ******************************************************************************/
void process_image_recon_request (ICPRECEIVEDDATA::ReceivedData& in_data,
                                  uint32_t&                      stream
                                  MSG_QUEUE                      out_mq)
{
  MESSAGE_STRUCT    msg_struct;
  ISMRMRD::Command  cmd;

  msg_struct = (*in_msg).front();
  (*in_msg).pop();

  if (msg_struct.ehdr.entity_type != ISMRMRD::ISMRMRD_COMMAND)
  {
    std::cout << "Error: Reconstruction request must start with a command" <<
                 std::endl;
    std::cout << "Entity: " <<
                 (ISMRMRD::EntityType) msg_struct.ehdr.entity_type << std::endl;

    queue_command_msg (out_mq,
                       ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER,
                       ISMRMRD::ISMRMRD_ERROR_INPUT_DATA_ERROR);
    return;
  }

  cmd.deserialize (std::vector<unsigned char>(msg_struct.data.begin(),
                                              msg_struct.data.end()));

  if (cmd.command_type != ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION)
  {
    std::cout << "Error: command type doesnt match the task" << std::endl;
    std::cout << "This task: ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION"  << std::endl;
    std::cout << "Requested: " << (ISMRMRD::CommandType) cmd.command_type <<
                 std::endl;

    queue_command_msg (out_mq,
                       ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER,
                       ISMRMRD::ISMRMRD_ERROR_INTERNAL_ERROR);
    return;
  }


  // Now we expect the ISMRMRD XML Header
  msg_struct = (*in_msg).front();
  (*in_msg).pop();

  // There is nothing to change in the xml header for this demo but send it back
  queue_xml_header_msg ((*msg_struct).data, out_mq);

  ISMRMRD::IsmrmrdHeader hdr;
  ISMRMRD::deserialize((*msg_struct).data, hdr); // needs to be redone

  ISMRMRD::EncodingSpace e_space = hdr.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = hdr.encoding[0].reconSpace;
  uint16_t eX = e_space.matrixSize.x;
  uint16_t eY = e_space.matrixSize.y;
  //uint16_t eZ = e_space.matrixSize.z;
  uint16_t rX = r_space.matrixSize.x;
  uint16_t rY = r_space.matrixSize.y;
  uint16_t rZ = r_space.matrixSize.z;
  
  // Get first acquisition to read the number of coils
  msg_struct = (*in_msg).front(); // Fix
  (*in_msg).pop();

  ISMRMRD::EntityType e_type =
    (ISMRMRD::EntityType)((*msg_struct).ehdr.entity_type);
  ISMRMRD::StorageType e_type =
    (ISMRMRD::StorageType)((*msg_struct).ehdr.entity_type);
  uint32_t stream = (*msg_struct).ehdr.stream;



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
      queue_error_command (out_msg, ISMRMRD::ISMRMRD_ERROR_INTERNAL_ERROR);
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
void read_socket
(
  socket_ptr sock,
  int        id
)
{
  std::cout << "Reader thread (" << id << ") started\n" << std::flush;

  // Start the writer thread
  MSG_QUEUE        out_mq;
  std::thread writer (write_socket, sock, out_mq, id);

  ICPRECEIVEDDATA::ReceivedData in_data (new ICPRECEIVEDDATA::ReceivedData);

  // Receive and authenticate the Handshake
  ISMRMRD::Handshake handshake;
  receive_handshake (sock, handshake);

  ISMRMRD::ConnectionStatus conn_status;

  printf ("About to autenticate\n");

  if ((conn_status = authenticate_client (handshake)) !=
       ISMRMRD::CONNECTION_ACCEPTED)
  {
    printf ("About to reject\n");
    reject_connection (out_mq,
                       handshake.timestamp,
                       conn_status,
                       handshake.client_name);
    writer.join();
    return;
  }

  printf ("About to queue handshake response\n");
  queue_handshake_msg (handshake, out_mq);


  // After the handshake exchange - keep reading messages in until the
  // STOP_FROM_CLIENT command is received

  bool done = false;
  while (!done)
  {
    MSG_STRUCT msg_struct;
    receive_message (sock, msg_struct);

    if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_COMMAND)
    {
      ISMRMRD::Command cmd;
      cmd.deserialize (msg_struct.data);

      in_data.addCommand (msg_struct.ehdr, cmd);

      if (cmd.command_type == ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT)
      {
        done = true;
        // TODO: Specific processing at the end of communication
        std::cout << "RTeceived the STOP command from client" << std::endl;
      }
    }
    else if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_ERROR)
    {
      // TODO: Error specific processing
      // If needed - add the error to the Received data
    }
    else if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_XML_HEADER)
    {
      // TODO: XML Header specific processing here...
      //       This may include starting monitoring streams for completeness
      //       and as streams are complete, calling the processing routinesi
      //       or threads.
      in_data->addToStream (msg_struct.ehdr, msg_struct.data);
    }
    else if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_MRACQUISITION)
    {
      in_data->addToStream (msg_struct.ehdr, msg_struct.data);
    }
    else if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_WAVEFORM)
    {
      in_data->addToStream (msg_struct.ehdr, msg_struct.data);
    }
    else if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_IMAGE)
    {
      in_data->addToStream (msg_struct.ehdr, msg_struct.data);
    }
    else if (msg_struct.ehdr.entity_type == ISMRMRD::ISMRMRD_BLOB)
    {
      in_data->addToStream (msg_struct.ehdr, msg_struct.data);
    }
    else
    {
      // Unexpected entity type. Report and continue processing
      std::cout << "Warning! unexpected Entity Type received: " <<
                   msg_struct.ehdr.entity_type << std::endl;
      continue;
    }
  }

  // Process the streams of received data that have not been 
  // processed yet. At this point in development all data is
  // processed here, but the plan is to monitor the streams of
  // data using the streams description in the XML header, and as
  // streams are getting complete - process them immediatelly.
  process_received_data (in_data, out_mq);


  printf ("reader is done, waiting for writer to join\n");
  writer.join();
  printf ("writer joined, reader exiting\n");

  return;

}


/*******************************************************************************
 ******************************************************************************/
int main (int argc, char* argv[])
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
      std::cout << "Accepted client (id " << id << "), starting communications...\n";
      std::thread (read_socket, sock, id).detach();
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "Main thread Exception: " << e.what() << "\n";
  }

  return 0;
}
