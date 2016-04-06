// client.cpp
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

using boost::asio::ip::tcp;
namespace po = boost::program_options;

const int __one__ = 1;
const bool isCpuLittleEndian = 1 == *(char*)(&__one__);

const unsigned char my_version = 2;

// Dataframe size is of type uint64_t
const size_t   DATAFRAME_SIZE_FIELD_SIZE  =  sizeof (uint64_t);

// Entity Header size with 0 padding is sizeof (uint32_t) times 4
const size_t   ENTITY_HEADER_SIZE         = sizeof (uint32_t) * 4;

struct INPUT_MESSAGE_STRUCTURE
{
  uint64_t                   size;
  ISMRMRD::EntityHeader      ehdr;
  uint32_t                   data_size;
  std::vector<unsigned char> data;
};
typedef std::shared_ptr<INPUT_MESSAGE_STRUCTURE>  IN_MSG;

struct OUTPUT_MESSAGE_STRUCTURE
{
  uint32_t                   size;
  std::vector<unsigned char> data;
};
typedef OUTPUT_MESSAGE_STRUCTURE OUT_MSG;
typedef std::shared_ptr<std::queue<OUT_MSG> >     OUTPUT_QUEUE;

typedef std::shared_ptr<tcp::socket> socket_ptr;
//const unsigned char stamp[4] = {'I', 'M', 'R', 'D' };

/*******************************************************************************
 ******************************************************************************/
int receiveFrameInfo
(
  socket_ptr sock,
  IN_MSG     in_msg
)
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
  //if (error != )
  //{
    //return 1;
  //}

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
bool processCommand
(
  ISMRMRD::Command& cmd
)
{
  bool success = true;
  if (cmd.command_type != ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTED)
  {
    std::cout << "Error: expected command " <<
                 ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTED << std::endl;
    success = false;
  }
  std::cout << "Received command: " << cmd.command_type << std::endl;

  return success;
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
int receiveHandshake
(
  socket_ptr          sock,
  ISMRMRD::Handshake& handshake
)
{
  IN_MSG                     in_msg;

  receiveMessage (sock, in_msg);

  if ((*in_msg).ehdr.entity_type != ISMRMRD::ISMRMRD_HANDSHAKE)
  {
    std::cout << "Error! Expected handshake - received entity " <<
                 (*in_msg).ehdr.entity_type << std::endl;
    return 1;
  }
 
  handshake.deserialize ((*in_msg).data);

  std::cout << "Timestamp            :" << handshake.timestamp << std::endl;
  std::cout << "Client name          :" << handshake.client_name << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void readSocket
(
  socket_ptr  sock,
  std::string my_name,
  std::string out_fname,
  std::string dset_name
)
{
  std::cout << "\nClient Reader started\n" << std::endl;

  boost::system::error_code  error;
  ISMRMRD::EntityHeader      e_hdr;
  ISMRMRD::Handshake         handshake;
  ISMRMRD::Command           cmd;
  std::string                xml_head_string;
  ISMRMRD::IsmrmrdHeader     xmlHeader;
  std::vector<unsigned char> img_buf;
  bool                       xml_hdr_received = false;
  bool                       image_received   = false;
  bool                       done             = false;

  receiveHandshake (sock, handshake);
  if (my_name != handshake.client_name)
  {
    std::cout << "Error: received handshake for different client:" << std::endl;
    std::cout << "My name  :" << my_name << std::endl;
    std::cout << "Intended :" << handshake.client_name << std::endl;
  }

  while (!done)
  {
    IN_MSG in_msg;
    receiveMessage (sock, in_msg);

    switch ((*in_msg).ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize ((*in_msg).data);
        if (cmd.command_type == ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER)
        {
          done = true;
        }
        else
        {
          processCommand (cmd);
        }
        break;

      case ISMRMRD::ISMRMRD_ERROR:

        // TODO: Error specific processing here...
        // Consider adding to the Received data
        break;

      case ISMRMRD::ISMRMRD_XML_HEADER:

        // TODO: XML Header specific processing here...
        ISMRMRD::deserialize ((const char*) &(*in_msg).data[0], xmlHeader);
        xml_hdr_received = true;
        break;

      case ISMRMRD::ISMRMRD_IMAGE:

        img_buf = (*in_msg).data;
        image_received = true;
        break;

      default:

        // TODO: Throw Unexpected entity type error.
        std::cout << "Warning! unexpected Entity Type received: " <<
                     (*in_msg).ehdr.entity_type << std::endl;
        break;

    } // switch ((*in_msg).ehdr.entity_type)
  } // while (true)

  if (!xml_hdr_received)
  {
    std::cout << "No XML header received, exiting" << std::endl;
    return;
  }
  else if (!image_received)
  {
    std::cout << "No image received, exiting" << std::endl;
    return;
  }

  ISMRMRD::EncodingSpace r_space = xmlHeader.encoding[0].reconSpace;
  ISMRMRD::Image<float> img (r_space.matrixSize.x,
                             r_space.matrixSize.y,
                             r_space.matrixSize.z,
                             1); // num channels ???
  img.deserialize (img_buf);

  ISMRMRD::Dataset dset (out_fname.c_str(), dset_name.c_str(), true);
  dset.writeHeader (xml_head_string);
  // TODO:
  dset.appendImage ("cpp", img);
  // appendImage is not yet implemented for version 2.x
  // Consider implementing a version for appending serialized image

  std::cout << "\nFinished processing the data - exiting" << std::endl;
  return;
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
  printf ("queueMessage 1\n");
  OUT_MSG msg;
  printf ("queueMessage 2\n");
  msg.size = size;
  printf ("queueMessage 3\n");
  msg.data.reserve (size);

  std::cout << "Queueing message of size = " << size << std::endl;
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));
  printf ("data size = %ld, sizeof = %ld\n", data.size(), sizeof (data));

  uint64_t s = size;
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  std::cout << "Size = " << size << std::endl;
  std::cout << "Sizeof s = " << sizeof (s)  << std::endl;

  std::copy ((unsigned char*) &s,
             (unsigned char*) &s + sizeof (s),
             std::back_inserter (msg.data));
  printf ("queueMessage 4\n");

  std::copy ((unsigned char*) &ent,
             (unsigned char*) &ent + ent.size(),
             std::back_inserter (msg.data));
  printf ("queueMessage 5\n");

  std::copy ((unsigned char*) &data,
             (unsigned char*) &data + data.size(),
             std::back_inserter (msg.data));
  printf ("queueMessage 6\n");

  oq->push (msg);
  printf ("queueMessage 7\n");

  return;
}

/*******************************************************************************
 ******************************************************************************/
void queueHandshake
(
  std::string  client_name,
  OUTPUT_QUEUE    out_mq
)
{
  printf ("queueHandshake 1\n");
  struct timeval tv;
  gettimeofday(&tv, NULL);

  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65536;
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  ISMRMRD::Handshake handshake;
  handshake.timestamp = (uint64_t)(tv.tv_sec);
  memset (handshake.client_name, 0, ISMRMRD::Max_Client_Name_Length);
  strncpy (handshake.client_name, client_name.c_str(), client_name.size());

  std::vector<unsigned char> hand = handshake.serialize();
  printf ("hand size = %ld, sizeof = %ld\n", hand.size(), sizeof (hand));

  uint64_t  size = (uint64_t) (ent.size() + hand.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  std::cout << "Size = " << size << std::endl;

  printf ("queueHandshake 2\n");
  queueMessage (size, ent, hand, out_mq);
  printf ("queueHandshake 3\n");
}

/*******************************************************************************
 ******************************************************************************/
void queueXmlHeader
(
  std::string xml_head,
  OUTPUT_QUEUE out_mq
)
{
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_XML_HEADER;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 1;
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  std::vector<unsigned char> xml;
  xml.reserve (xml_head.size());
  memcpy ((void*)&xml_head[0], (void*)&xml[0], xml_head.size());

  uint64_t size  = (uint64_t) (ent.size() + xml_head.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, xml, out_mq);
}

/*******************************************************************************
 ******************************************************************************/
void queueCommand (ISMRMRD::Command& cmd, OUTPUT_QUEUE out_mq)
{
  printf ("queueCommand 1\n");
  ISMRMRD::EntityHeader e_hdr;
  printf ("queueCommand 2\n");
  e_hdr.version = my_version;
  printf ("queueCommand 3\n");
  e_hdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
  printf ("queueCommand 4\n");
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  printf ("queueCommand 5\n");
  e_hdr.stream = 65537; // Todo: define reserved stream number names
  printf ("queueCommand 6\n");
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  std::vector<unsigned char> command = cmd.serialize();
  printf ("command size = %ld, sizeof = %ld\n", command.size(), sizeof (command));

  uint64_t size = (uint64_t) (ent.size() + command.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  printf ("queueCommand 7\n");
  queueMessage (size, ent, command, out_mq);
  printf ("queueCommand 8\n");
}

/*******************************************************************************
 ******************************************************************************/
template <typename T>
void queueAcquisition
(
  ISMRMRD::Acquisition<T> acq,
  OUTPUT_QUEUE out_mq
)
{
  ISMRMRD::EntityHeader e_hdr;

  e_hdr.version = my_version;
  e_hdr.entity_type  = ISMRMRD::ISMRMRD_MRACQUISITION;
  e_hdr.storage_type = acq.getStorageType();
  e_hdr.stream       = acq.getStream();
  std::vector<unsigned char> ent = e_hdr.serialize();
  printf ("ent size = %ld, sizeof = %ld\n", ent.size(), sizeof (ent));

  std::vector<unsigned char> acquisition = acq.serialize();

  uint64_t size = (uint64_t) (ent.size() + acquisition.size());
  std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, acquisition, out_mq);
}

/*******************************************************************************
 ******************************************************************************/
void prepareDataQueue (std::string         fname,
                       std::string         dname,
                       OUTPUT_QUEUE        out_mq,
                       std::string         client_name,
                       std::atomic<bool>*  done)
{
  printf ("prepareDataQueue 1, %s, %s\n", fname.c_str(), dname.c_str());
  ISMRMRD::Dataset dset (fname.c_str(), dname.c_str(), false);
  std::string xml_head = dset.readHeader();
  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);

  queueHandshake (client_name, out_mq);

  ISMRMRD::Command cmd;
  //std::vector<uint32_t> test;
  //test.push_back (42);
  std::cout << "prepareDataQueue 1.2 " << std::endl;
  cmd.command_id = 1;
  cmd.command_type = ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION;
  cmd.num_streams = 1;
  //std::cout << "prepareDataQueue 1.3 " << xmlHeader.streams[0].number << std::endl;
  cmd.streams.resize(2);
  std::cout << "prepareDataQueue 1.3.1 " << std::endl;
  cmd.streams.push_back (42);
  //std::cout << "prepareDataQueue 1.3.2 " << xmlHeader.streams[0].number << std::endl;
  //cmd.streams.push_back (xmlHeader.streams[0].number);
  printf ("prepareDataQueue 1.4\n");
  cmd.data_count.push_back
    (dset.getNumberOfAcquisitions (xmlHeader.streams[0].number));
  printf ("prepareDataQueue 2\n");
  cmd.config_size = 0;
  printf ("prepareDataQueue 3\n");
  queueCommand (cmd, out_mq);
  printf ("prepareDataQueue 4\n");

  queueXmlHeader (xml_head, out_mq);
  printf ("prepareDataQueue 5\n");
  
  int num_acq = dset.getNumberOfAcquisitions();

  std::cout << "Acq storage type is: " <<
               dset.readAcquisition<float>(0).getStorageType() << std::endl;

  printf ("prepareDataQueue 6\n");
  for (int ii = 0; ii < num_acq; ++ii)
  {
    queueAcquisition (dset.readAcquisition<float> (ii), out_mq);
  }

  printf ("prepareDataQueue 7\n");
  cmd.command_id = 2;
  cmd.command_type = ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT;
  cmd.num_streams = 0;
  cmd.streams.clear();
  cmd.data_count.clear();
  cmd.config_size = 0;
  queueCommand (cmd, out_mq);

  printf ("prepareDataQueue 8\n");
  *done = true;
}

/*******************************************************************************
 ******************************************************************************/
int main (int argc, char* argv[])
{
  std::string       client_name = "HAL 9000";
  std::string       host        = "127.0.0.1";
  std::string       in_fname    = "FileIn.h5";
  std::string       in_dset     = "dataset";
  std::string       out_fname   = "FileOut.h5";
  std::string       out_dset    = "dataset";
  unsigned short    port        = 50050;
  struct timespec   ts;

  ts.tv_sec  =    0;
  ts.tv_nsec = 1000;
  

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("client name,c", po::value<std::string>(&client_name)->default_value(client_name), "Client Name")
    ("host,H", po::value<std::string>(&host)->default_value(host), "Server IP address")
    ("port,p", po::value<unsigned short>(&port)->default_value(port), "TCP port for server")
    ("fin,i", po::value<std::string>(&in_fname)->default_value(in_fname), "HDF5 Input file")
    ("in_group,I", po::value<std::string>(&in_dset)->default_value(in_dset), "Input group name")
    ("fout,o", po::value<std::string>(&out_fname)->default_value(out_fname), "Output file name")
    ("out_group,O", po::value<std::string>(&out_dset)->default_value(out_dset), "Output group name")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cout << desc << "\n";
    return 0;
  }

  std::cout << "Using client name [" << client_name << "], host IP address [" << host <<
               "], and port [" << port << "]" << std::endl;
  std::cout << "To change re-start with: icli -h" << std::endl;


  // Spawn a thread to prepare the data to be sent to the server
  std::cout << "Attempting to prepare data from " << in_fname << std::endl;
  std::atomic<bool> done (false);
  OUTPUT_QUEUE      out_mq (new std::queue<OUT_MSG>);
  printf ("Main 1\n");
  std::thread t1 (prepareDataQueue,
                  in_fname,
                  in_dset,
                  out_mq,
                  client_name,
                  &done);
  printf ("Main 2\n");

  try
  {
  printf ("Main 3\n");
    boost::asio::io_service io_service;
    socket_ptr sock (new tcp::socket (io_service));
    tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);
  printf ("Main 4\n");

    boost::system::error_code error = boost::asio::error::host_not_found;
    (*sock).connect(endpoint, error);
    if (error) throw boost::system::system_error(error);
  printf ("Main 5\n");

    std::thread t2 (readSocket, sock, client_name, out_fname, out_dset);
    sleep(1);
  printf ("Main 6\n");

    while (!done || !(*out_mq).empty())
    {
  printf ("Main 7\n");
      if (out_mq->empty())
      {
  printf ("Main 7.1\n");
        nanosleep (&ts, NULL);
  printf ("Main 7.2\n");
        continue;
      }

  printf ("Main 8\n");
      OUT_MSG msg = out_mq->front();
      out_mq->pop();
      boost::asio::write (*sock, boost::asio::buffer (msg.data, msg.size));
      std::cout << client_name << " sent out a frame" << std::endl;
    }

    t1.join();
    t2.join();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
