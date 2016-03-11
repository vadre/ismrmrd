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

typedef std::shared_ptr<tcp::socket> socket_ptr;

struct message_struct
{
  uint64_t size;
  ISMRMRD::EntityHeader ehdr;
  std::string data;
};
typedef std::shared_ptr<std::queue<message_struct> > msg_struct_queue;

typedef std::shared_ptr<std::vector<unsigned char> > message;
typedef std::shared_ptr<std::queue<message> > msg_queue;

const unsigned char my_version = 2;
//const unsigned char stamp[4] = {'I', 'M', 'R', 'D'};

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
void write_socket (socket_ptr sock, msg_queue mq, int id)
{
  std::cout << "Writer thread (" << id << ") started\n" << std::flush;

  message_struct msg;
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
  msg_queue                  mq,
  ISMRMRD::Handshake         handshake
)
{
  queue_handshake_msg (mq,
                       handshake.timestamp,
                       handshake.conn_status,
                       handshake.client_name);

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
  msg_queue mq
)
{
  message msg;
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
  ISMRMRD::Handshake handshake,
  msg_queue          mq,
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

  handshake.conn_status = (uint32_t) ISMRMRD::CONNECTION_ACCEPTED;
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
int receive_frame_info (socket_ptr sock, message_struct& msg)
{
  boost::system::error_code  error;
  boost::asio::read (*sock, boost::asio::buffer (&msg.size, sizeof (uint64_t)), error);

  std::cout << "frame_size read status: " << error << std::endl;
  std::cout << "frame_size            : " << msg.size << std::endl;

  boost::asio::read (*sock, boost::asio::buffer (&msg.ehdr, sizeof (msg.ehdr)), error);
  std::cout << "Entity header read status: " << error << std::endl;
  std::cout << "Entity header size       : " << sizeof (ISMRMRD::EntityHeader) << std::endl;
  std::cout << "version                  : " << std::to_string (msg.ehdr.version) << std::endl;
  std::cout << "entity_type              : " << (ISMRMRD::EntityType) msg.ehdr.entity_type << std::endl;
  std::cout << "storage_type             : " << (ISMRMRD::StorageType) msg.ehdr.storage_type << std::endl;
  std::cout << "stream                   : " << msg.ehdr.stream << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void receive_handshake (socket_ptr            sock,
                        msg_struct_queue             in_msg,
                        ISMRMRD::Handshake&   handshake)
{
  boost::system::error_code  error;
  message_struct msg;
  //ISMRMRD::Handshake handshake;

  receive_frame_info (sock, msg);

  boost::asio::read (*sock,
                     boost::asio::buffer (&msg.data[0],
                                          sizeof (ISMRMRD::Handshake)),
                     error);

  handshake.deserialize (std::vector<unsigned char> (msg.data.begin(),
                                                     msg.data.end()));

  std::cout << "Handshake read status: " << error << std::endl;
  std::cout << "Handshake struct size: " << sizeof (ISMRMRD::Handshake) << std::endl;
  std::cout << "Timestamp            :" << handshake.timestamp << std::endl;
  std::cout << "Client name          :" << handshake.client_name << std::endl;

  //client_name = handshake.client_name;
  //msg.data = (char*)&(handshake.serialize())[0];
  (*in_msg).push (msg);

  return;

}

/*******************************************************************************
 ******************************************************************************/
void receive_command (socket_ptr sock, msg_struct_queue in_msg, ISMRMRD::CommandType& cmd_type)
{
  boost::system::error_code  error;
  message_struct msg;
  ISMRMRD::Command cmd;

  receive_frame_info (sock, msg);

  boost::asio::read (*sock, boost::asio::buffer (&cmd, sizeof (ISMRMRD::Command)), error);
  std::cout << "Command read status: " << error << std::endl;
  std::cout << "Command struct size: " << sizeof (ISMRMRD::Command) << std::endl;
  std::cout << "Command            : " << (ISMRMRD::CommandType) cmd.command_type << std::endl;

  cmd_type = (ISMRMRD::CommandType) cmd.command_type;
  msg.data = (char*)&(cmd.serialize())[0];
  (*in_msg).push (msg); 
  
  return;
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
  msg_struct_queue&             in_msg
)
{
  boost::system::error_code  error;
  ISMRMRD::Command           cmd;

  do
  {
    message_struct msg;
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
  msg_queue             mq,
  ISMRMRD::ErrorType    err_type = ISMRMRD::ISMRMRD_ERROR_NO_ERROR,
  ISMRMRD::CommandType  cmd_type = ISMRMRD::ISMRMRD_COMMAND_NO_COMMAND
)
{
  message           msg;
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
void process_image_recon_request (msg_struct_queue& in_msg, msg_queue out_mq)
{
  message_struct msg;
  ISMRMRD::Command cmd;

  msg = (*in_msg).front();
  (*in_msg).pop();
  if (msg.ehdr.entity_type != ISMRMRD::ISMRMRD_COMMAND)
  {
    std::cout << "Error: input queue must start with a command" << std::endl;
    std::cout << "Entity: " << (ISMRMRD::EntityType) msg.ehdr.entity_type << std::endl;
    queue_command_msg (out_mq,
                       ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER,
                       ISMRMRD::ISMRMRD_ERROR_INPUT_DATA_ERROR);
    return;
  }

  cmd.deserialize (std::vector<unsigned char>(msg.data.begin(), msg.data.end()));

  if (cmd.command_type != ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION)
  {
    std::cout << "Error: command type doesnt match the task" << std::endl;
    std::cout << "This task: ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION"  << std::endl;
    std::cout << "Requested: " << (ISMRMRD::CommandType) cmd.command_type << std::endl;
    queue_command_msg (out_mq, ISMRMRD::ISMRMRD_ERROR_INTERNAL_ERROR);
    return;
  }


  msg = (*in_msg).front();
  (*in_msg).pop();
  (*out_msg).push (msg);
  ISMRMRD::IsmrmrdHeader hdr;
  ISMRMRD::deserialize(msg.data.c_str(), hdr);
  ISMRMRD::EncodingSpace e_space = hdr.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = hdr.encoding[0].reconSpace;
  uint16_t eX = e_space.matrixSize.x;
  uint16_t eY = e_space.matrixSize.y;
  //uint16_t eZ = e_space.matrixSize.z;
  uint16_t rX = r_space.matrixSize.x;
  uint16_t rY = r_space.matrixSize.y;
  uint16_t rZ = r_space.matrixSize.z;
  
  // Get first acquisition to read the number of coils
  msg = (*in_msg).front();
  (*in_msg).pop();
  ISMRMRD::Acquisition<float> acq;
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
void read_socket (socket_ptr sock, int id)
{
  std::cout << "Reader thread (" << id << ") started\n" << std::flush;

  msg_struct_queue in_msg (new std::queue<message_struct>);
  //msg_struct_queue out_msg (new std::queue<message_struct>);

  msg_queue out_mq;

  std::thread writer (write_socket, sock, out_mq, id);

  std::string client_name;
  ISMRMRD::Handshake handshake;
  receive_handshake (sock, in_msg, handshake);

  ISMRMRD::ConnectionStatus conn_status;

  printf ("About to autenticate\n");

  if ((conn_status = authenticate_client (handshake)) !=
       ISMRMRD::CONNECTION_ACCEPTED)
  {
    printf ("About to reject\n");
    handshake.conn_status = conn_status;
    reject_connection (out_mq, handshake);
    writer.join();
    return;
  }

  printf ("About to queue handshake response\n");
  queue_handshake_msg (handshake, out_mq);

  printf ("About to receive command\n");
  ISMRMRD::CommandType cmd_type;
  receive_command (sock, in_msg, cmd_type);

  if (cmd_type != ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION)
  {
    printf ("Received unexpected command\n");
    // Probably need to let the client know that the expected operation can't be executed
    std::cout << "The use case for this command does not exist, the received data is discarded." << std::endl;
    writer.join();
    return;
  }
  printf ("Received command, about to receive data\n");

  receive_image_recon_data (sock, in_msg);

  printf ("About to receive the recon request command\n");
  process_image_recon_request (in_msg, out_mq);

  printf ("reader is done, waiting for writer to join\n");
  writer.join();
  printf ("writer joined, reader exiting\n");

  return;

  /*
  try
  {
    for (;;)
    {
      char data[max_length];

      boost::system::error_code error;
      size_t length = sock->read_some (boost::asio::buffer (data), error);

      messages->push (std::string (data));

      std::cout << "Reader (" << id << ") received: [" << data << "]" << std::endl;
      //std::cout.write (data, length);

      if (!std::strncmp (data, "Client Command Stop", std::strlen(data)))
      {
        std::cout << "Reader (" << id << ") received the Stop command, stopping reading\n";
        break;
      }

      if (error == boost::asio::error::eof)
      {
        std::cout << "Reader (" << id << "): Num bytes received = " << length << std::endl;
        std::cout << "Reader (" << id << "): Client disconnected, closing socket and exiting\n";
        break; // Connection closed cleanly by peer.
      }
      else if (error)
      {
        throw boost::system::system_error (error); // Some other error.
      }

    }

    writer.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Reader (" << id << "): Exception: " << e.what() << "\n";
  }
  */
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
