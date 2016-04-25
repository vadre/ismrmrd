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
//typedef std::shared_ptr<INPUT_MESSAGE_STRUCTURE>  IN_MSG;
typedef INPUT_MESSAGE_STRUCTURE  IN_MSG;

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
  IN_MSG&    in_msg
)
{
  //int step = 0;
  //std::cout << __FILE__ << ":" << __func__ << step++ << '\n';

  int bytes_read = 0;
  boost::system::error_code  error;

  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&in_msg.size,
                                            DATAFRAME_SIZE_FIELD_SIZE),
                       error);

  if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    printf ("Read wrong num bytes for size: %d\n", bytes_read);
  }
  if (error)
  {
    // TODO: Throw an error
    std::cout << "frame_size read status: " << error << std::endl;
    return 1;
  }

  printf ("receiveFrameInfo: frame size: %llu\n", in_msg.size);

  std::vector<unsigned char> buffer (ENTITY_HEADER_SIZE);

  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&buffer[0],
                                            ENTITY_HEADER_SIZE),
                       error);
  if (error)
  {
    // TODO: Throw an error
    std::cout << "EntityHeader read status: " << error << std::endl;
    return 1;
  }
  if (bytes_read != ENTITY_HEADER_SIZE)
  {
    printf ("Read wrong num bytes for entity header: %d\n", bytes_read);
  }

  in_msg.ehdr.deserialize (buffer);

  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;

  printf ("data size = %u\n\n", in_msg.data_size);

  //std::cout << __FILE__ << ":" << __func__ << step++ << '\n';

  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;
  //std::cout << __FILE__ << ":" << __func__ << step++ << '\n';

  std::cout << "version                 : " <<
               std::to_string (in_msg.ehdr.version) << std::endl;
  std::cout << "entity_type             : " <<
               (ISMRMRD::EntityType) in_msg.ehdr.entity_type <<
               std::endl;
  std::cout << "storage_type            : " <<
               (ISMRMRD::StorageType) in_msg.ehdr.storage_type <<
               std::endl;
  std::cout << "stream                  : " <<
               in_msg.ehdr.stream << std::endl;
  std::cout << "data size               : " <<
               in_msg.data_size << std::endl;

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
  if (cmd.command_type != ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER)
  {
    std::cout << "Error: expected command " <<
                 ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER << std::endl;
    success = false;
  }
  std::cout << "Received command: " << cmd.command_type << std::endl;

  return success;
}

/*******************************************************************************
 ******************************************************************************/
void receiveMessage (socket_ptr sock, IN_MSG& in_msg)
{
  boost::system::error_code  error;

  if (receiveFrameInfo (sock, in_msg))
  {
    std::cout << __func__ << ": Error from receiveFrameInfo!\n";
    return;
  }

  if (in_msg.data_size <= 0)
  {
    // TODO: Throw an error (unless we can have an entity_header as a command ??)
    std::cout << "Error! The data_size = " << in_msg.data_size << std::endl;
    std::cout << "EntityType = " << in_msg.ehdr.entity_type << std::endl;
    return;
  }

  in_msg.data.resize (in_msg.data_size);
  int bytes_read = boost::asio::read (*sock,
                                      boost::asio::buffer (&in_msg.data[0],
                                      in_msg.data_size),
                                      error);

  if (bytes_read != in_msg.data.size())
  {
    printf ("Error: Read wrong num bytes for data: %d, instead of %lu\n",
            bytes_read, in_msg.data.size());
  }
  if (error)
  {
    // TODO: Throw an error
    std::cout << "Error: message data read status: " << error << std::endl;
  }

  std::cout << __func__ << ": Entity = " << in_msg.ehdr.entity_type <<
               ", size = " << in_msg.data_size << '\n';
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
  struct timespec   ts;
  ts.tv_sec  =    0;
  ts.tv_nsec = 5000000;

  nanosleep (&ts, NULL);

  while (!done)
  {
    IN_MSG in_msg;
    in_msg.size = 0;
    receiveMessage (sock, in_msg);
    if (in_msg.size == 0)
    {
      std::cout << __func__ << ": message size 0, exiting loop\n";
      break;
    }

    switch (in_msg.ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_HANDSHAKE:

        handshake.deserialize (in_msg.data);
        std::cout << "Timestamp  :" << handshake.timestamp   << std::endl;
        std::cout << "Client name:" << handshake.client_name << std::endl;
        std::cout << "Status     :" << handshake.conn_status << std::endl;
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        printf ("Deserializing command\n");
        cmd.deserialize (in_msg.data);
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

        std::cout << __func__ << ": Got xml header\n";
        xml_head_string = std::string (in_msg.data.begin(), in_msg.data.end());
        
        ISMRMRD::deserialize (xml_head_string.c_str(), xmlHeader);
        //ISMRMRD::deserialize ((const char*) &in_msg.data[0], xmlHeader);
        xml_hdr_received = true;
        break;

      case ISMRMRD::ISMRMRD_IMAGE:

        std::cout << __func__ << ": Got image\n";
        img_buf = in_msg.data;
        image_received = true;
        break;

      default:

        // TODO: Throw Unexpected entity type error.
        std::cout << "Warning! unexpected Entity Type received: " <<
                     in_msg.ehdr.entity_type << std::endl;
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

  int step = 100;
  std::cout << __func__ << ": " << step++ << "\n";

  ISMRMRD::EncodingSpace r_space = xmlHeader.encoding[0].reconSpace;
  ISMRMRD::Image<float> img (r_space.matrixSize.x,
                             r_space.matrixSize.y,
                             r_space.matrixSize.z,
                             1); // num channels ???
  std::cout << __func__ << ": " << step++ << "\n";
  img.deserialize (img_buf);
  std::cout << "getMatrixSizeX() = " << img.getMatrixSizeX() << '\n';
  std::cout << "getMatrixSizeY() = " << img.getMatrixSizeY() << '\n';
  std::cout << "getMatrixSizeZ() = " << img.getMatrixSizeZ() << '\n';

  std::cout << __func__ << ": " << step++ << "\n";
  //ISMRMRD::Dataset dset (out_fname.c_str(), dset_name.c_str(), true);
  ISMRMRD::Dataset dset (out_fname.c_str(), dset_name.c_str());
  std::cout << __func__ << ": " << step++ << "\n";
  dset.writeHeader (xml_head_string);
  // TODO:
  std::cout << __func__ << ": " << step++ << "\n";
  dset.appendImage ("cpp", img);
  // appendImage is not yet implemented for version 2.x
  // Consider implementing a version for appending serialized image
  std::cout << __func__ << ": " << step++ << "\n";

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
  OUT_MSG msg;
  msg.size = size;
  msg.data.reserve (size);

  //printf ("Queueing message of size %u (%lu + %lu)",
          //size, ent.size(), data.size());

  uint64_t s = size;
  //std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  //std::cout << "Size = " << size << std::endl;

  std::copy ((unsigned char*) &s,
             (unsigned char*) &s + sizeof (s),
             std::back_inserter (msg.data));

  std::copy ((unsigned char*) &ent[0],
             (unsigned char*) &ent[0] + ent.size(),
             std::back_inserter (msg.data));

  std::copy ((unsigned char*) &data[0],
             (unsigned char*) &data[0] + data.size(),
             std::back_inserter (msg.data));

  //std::cout << "serialized out_msg: " <<  std::endl;
  //for (int ii = 0; ii != msg.data.size(); ii++) printf ("%x ", msg.data[ii]);
  //std::cout << std::endl;

  oq->push (msg);
  //printf ("  -  Done\n");

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
  struct timeval tv;
  gettimeofday(&tv, NULL);

  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65536;

  //printf ("Handshake entity header:\n");
  //printf ("version = %u, %x\n", e_hdr.version, e_hdr.version);
  //printf ("entity  = %u, %x\n", e_hdr.entity_type, e_hdr.entity_type);
  //printf ("storage = %u, %x\n", e_hdr.storage_type, e_hdr.storage_type);
  //printf ("stream  = %u, %x\n", e_hdr.stream, e_hdr.stream);
  std::vector<unsigned char> ent = e_hdr.serialize();

  //std::cout << "serialized: " <<  std::endl;
  //for (int ii = 0; ii != ent.size(); ii++) printf ("%x ", ent[ii]); 
  //std::cout << std::endl;

  ISMRMRD::Handshake handshake;
  handshake.timestamp = (uint64_t)(tv.tv_sec);
  memset (handshake.client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
  strncpy (handshake.client_name, client_name.c_str(), client_name.size());

  printf ("Handshake:\n");
  printf ("timestamp   = %llu\n", handshake.timestamp);
  printf ("client_name = %s\n", handshake.client_name);

  std::vector<unsigned char> hand = handshake.serialize();
  //std::cout << "serialized: " << std::endl;
  //for (int ii = 0; ii != hand.size(); ++ii) printf ("%x ", hand[ii]);
  //std::cout << std::endl;

  //printf ("ent size = %ld, hand size = %ld\n",  ent.size(), hand.size());

  uint64_t  size = (uint64_t) (ent.size() + hand.size());
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  //std::cout << "Size = " << size << std::endl;

  queueMessage (size, ent, hand, out_mq);
  printf ("Finished queueHandshake\n\n");
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

  std::vector<unsigned char> xml (xml_head.begin(), xml_head.end());
  //xml.reserve (xml_head.size());
  //memcpy ((void*)&xml_head[0], (void*)&xml[0], xml_head.size());

  uint64_t size  = (uint64_t) (ent.size() + xml.size());
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);
  printf ("queueXmlHeader: Size = %llu (%ld + %ld)\n",
          size, ent.size(), xml.size());

  queueMessage (size, ent, xml, out_mq);
}

/*******************************************************************************
 ******************************************************************************/
void queueCommand (ISMRMRD::Command& cmd, OUTPUT_QUEUE out_mq)
{
  printf ("In queueCommand... \n");
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65537; // Todo: define reserved stream number names
  std::vector<unsigned char> ent = e_hdr.serialize();

  std::vector<unsigned char> command = cmd.serialize();
  printf ("ent size = %ld, command size = %ld\n", ent.size(), command.size());

  uint64_t size = (uint64_t) (ent.size() + command.size());
  //std::cout << "Size = " << size << std::endl;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, command, out_mq);
  printf ("queueCommand Finished\n\n");
}

/*******************************************************************************
 ******************************************************************************/
template <typename T>
void queueAcquisition
(
  ISMRMRD::Acquisition<T> acq,
  OUTPUT_QUEUE            out_mq,
  bool                    empty = false
)
{
  ISMRMRD::EntityHeader e_hdr;

  e_hdr.version      = my_version;
  e_hdr.entity_type  = ISMRMRD::ISMRMRD_MRACQUISITION;
  e_hdr.storage_type = acq.getStorageType();
  e_hdr.stream       = acq.getStream();
  std::vector<unsigned char> ent = e_hdr.serialize();

  std::vector<unsigned char> acquisition;
  uint64_t size = 0;
  if (empty)
  {
    size = (uint64_t) ent.size();
  }
  else
  {
    acquisition = acq.serialize();
    size = (uint64_t) (ent.size() + acquisition.size());
  }

  //printf ("queueAcq: size = %llu, (%ld + %ld)\n",
          //size, ent.size(), acquisition.size());
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
  printf ("prepareDataQueue for file %s, groupname: %s\n",
          fname.c_str(), dname.c_str());

  ISMRMRD::Dataset dset (fname.c_str(), dname.c_str());
  std::string xml_head = dset.readHeader();
  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);

  queueHandshake (client_name, out_mq);

  queueXmlHeader (xml_head, out_mq);
  printf ("prepareDataQueue: Queued XML Header\n");
  //std::cout << "xmlHeader.version = " << xmlHeader.version << "\n";
    //std::cout << "xmlHeader.encoding[0].encodedSpace.matrixSize.y = " <<
                 //xmlHeader.encoding[0].encodedSpace.matrixSize.y << '\n';
  
  int num_acq = dset.getNumberOfAcquisitions(0);

  std::cout << "Acq storage type is: " <<
               dset.readAcquisition<float>(0, 0).getStorageType() << std::endl;

  //for (int ii = 0; ii < 4; ++ii)
  for (int ii = 0; ii < num_acq; ++ii)
  {
    queueAcquisition (dset.readAcquisition<float> (ii, 0), out_mq);
  }
  //printf ("prepareDataQueue: finished acqs, sending empty...\n");
  //queueAcquisition (dset.readAcquisition<float> (num_acq - 1, 0), out_mq, true);
  //printf ("prepareDataQueue: queued the empty acq\n");

  ISMRMRD::Command cmd;
  cmd.command_id = 2;
  cmd.command_type = ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT;
  printf ("prepareDataQueue: queueing command to stop\n");
  queueCommand (cmd, out_mq);

  printf ("prepareDataQueue: Finished \n");
  *done = true;
}

/*******************************************************************************
 ******************************************************************************/
int main (int argc, char* argv[])
{
  std::string       client_name = "Client 1";
  std::string       host        = "127.0.0.1";
  //std::string       in_fname    = "test.h5";
  std::string       in_fname    = "testdata.h5";
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
  std::cout << "To change re-start with: icpClient -h" << std::endl;


  // Spawn a thread to prepare the data to be sent to the server
  std::cout << "Attempting to prepare data from " << in_fname << std::endl;
  std::atomic<bool> done (false);
  OUTPUT_QUEUE      out_mq (new std::queue<OUT_MSG>);
  std::thread t1 (prepareDataQueue,
                  in_fname,
                  in_dset,
                  out_mq,
                  client_name,
                  &done);
  try
  {
    boost::asio::io_service io_service;
    socket_ptr sock (new tcp::socket (io_service));
    tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);

    boost::system::error_code error = boost::asio::error::host_not_found;
    (*sock).connect(endpoint, error);
    if (error) throw boost::system::system_error(error);

    std::thread t2 (readSocket, sock, client_name, out_fname, out_dset);

    ts.tv_nsec  =  5000000;
    nanosleep (&ts, NULL);
    ts.tv_nsec  =    1000;

  printf ("Starting sending out...\n");

    int msg_count = 0;
    while (!done || !(*out_mq).empty())
    {
      if (out_mq->empty())
      {
        nanosleep (&ts, NULL);
        continue;
      }

      msg_count++;
      OUT_MSG msg = out_mq->front();
      out_mq->pop();
      boost::asio::write (*sock, boost::asio::buffer (msg.data, 8 + msg.size));
      //printf ("   ==> %s sent out a frame %d, size %u\n",
              //client_name.c_str(), msg_count, msg.size);
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
