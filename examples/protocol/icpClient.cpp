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

const int __one__ = 0x0001;
const bool isCpuLittleEndian = 1 == *(char*)(&__one__);

const unsigned char my_version = 2;

const size_t   DATAFRAME_SIZE_FIELD_SIZE = sizeof (uint64_t);
const size_t   ENTITY_HEADER_SIZE        = sizeof (uint32_t) * 4;

typedef std::shared_ptr<tcp::socket> socket_ptr;

struct INPUT_MESSAGE_STRUCTURE
{
  uint64_t                   size;
  ISMRMRD::EntityHeader      ehdr;
  uint32_t                   data_size;
  std::vector<unsigned char> data;
};
typedef INPUT_MESSAGE_STRUCTURE  IN_MSG;


struct OUTPUT_MESSAGE_STRUCTURE
{
  uint32_t                   size;
  std::vector<unsigned char> data;
};
typedef OUTPUT_MESSAGE_STRUCTURE OUT_MSG;
typedef std::shared_ptr<std::queue<OUT_MSG> > OUTPUT_QUEUE;


/*******************************************************************************
 ******************************************************************************/
int receiveFrameInfo
(
  socket_ptr sock,
  IN_MSG&    in_msg
)
{
  boost::system::error_code  error;

  int bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&in_msg.size,
                                            DATAFRAME_SIZE_FIELD_SIZE),
                       error);
  if (error)
  {
    std::cout << "frame_size read status: " << error << std::endl;
  }

  if (bytes_read == 0)
  {
    std::cout << "Received EOF from server\n";
    return 100; // TODO define
  }
  else if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    printf ("Read wrong num bytes for size: %d\n", bytes_read);
    return 1; // TODO: define
  }

  std::vector<unsigned char> buffer (ENTITY_HEADER_SIZE);

  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&buffer[0],
                                            ENTITY_HEADER_SIZE),
                       error);
  if (error)
  {
    std::cout << "EntityHeader read status: " << error << std::endl;
  }
  if (bytes_read != ENTITY_HEADER_SIZE)
  {
    // TODO: at this point communication is broken.
    printf ("Read wrong num bytes for entity header: %d\n", bytes_read);
    return 200;
  }

  in_msg.ehdr.deserialize (buffer);
  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;

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
  // Not expecting any commands here yet
  std::cout << "Received command: " << cmd.getCommandType() << std::endl;

  return success;
}

/*******************************************************************************
 ******************************************************************************/
bool receiveMessage (socket_ptr sock, IN_MSG& in_msg)
{
  boost::system::error_code  error;

  if (receiveFrameInfo (sock, in_msg))
  {
    return false;
  }

  if (in_msg.data_size <= 0)
  {
    std::cout << "Error! The data_size = " << in_msg.data_size << std::endl;
    std::cout << "EntityType = " << in_msg.ehdr.entity_type << std::endl;
    return false;
  }

  in_msg.data.resize (in_msg.data_size);
  int bytes_read = boost::asio::read (*sock,
                                      boost::asio::buffer (&in_msg.data[0],
                                      in_msg.data_size),
                                      error);

  if (error)
  {
    std::cout << "Error: message data read status: " << error << std::endl;
  }
  if (bytes_read != in_msg.data.size())
  {
    printf ("Error: Read wrong num bytes for data: %d, instead of %lu\n",
            bytes_read, in_msg.data.size());
    return false;
  }

  return true;
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
  std::cout << "Client Reader started\n" << std::endl;

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
  struct timespec            ts;

  ts.tv_sec  =    0;
  ts.tv_nsec = 5000000;

  nanosleep (&ts, NULL);

  while (!done)
  {
    IN_MSG in_msg;

    if (!receiveMessage (sock, in_msg))
    {
      // TODO: receive detailed status and report/throw
      break;
    }

    switch (in_msg.ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_HANDSHAKE:

        handshake.deserialize (in_msg.data);
        std::cout << "\nHandshake returned  :\n";
        std::cout << "Client name         :" << handshake.getClientName() << "\n";
        std::cout << "Timestamp           :" << handshake.getTimestamp()   << "\n";
        std::cout << "Status              :" << handshake.getConnectionStatus() << "\n\n";
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize (in_msg.data);
        if (cmd.getCommandType() == ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER)
        {
          done = true;
          std::cout << "Received DONE from server\n";
        }
        else
        {
          processCommand (cmd);
        }
        break;

      case ISMRMRD::ISMRMRD_ERROR:

        // TODO: Error specific processing here...
        break;

      case ISMRMRD::ISMRMRD_XML_HEADER:

        xml_head_string = std::string (in_msg.data.begin(), in_msg.data.end());
        ISMRMRD::deserialize (xml_head_string.c_str(), xmlHeader);
        xml_hdr_received = true;
        std::cout << "Got xml header " << in_msg.data.size() << " bytes\n";
        break;

      case ISMRMRD::ISMRMRD_IMAGE:

        img_buf = in_msg.data;
        image_received = true;
        std::cout << "Got image " << img_buf.size() << " bytes\n";
        break;

      default:

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

  ISMRMRD::EncodingSpace r_space = xmlHeader.encoding[0].reconSpace;
  ISMRMRD::Image<float> img (r_space.matrixSize.x,
                             r_space.matrixSize.y,
                             r_space.matrixSize.z,
                             1);
  img.deserialize (img_buf);

  ISMRMRD::Dataset dset (out_fname.c_str(), dset_name.c_str());
  dset.writeHeader (xml_head_string);

  // TODO:  appendImage is not yet implemented for version 2.x
  dset.appendImage ("cpp", img);

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

  uint64_t s = size;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  std::copy ((unsigned char*) &s,
             (unsigned char*) &s + sizeof (s),
             std::back_inserter (msg.data));

  std::copy ((unsigned char*) &ent[0],
             (unsigned char*) &ent[0] + ent.size(),
             std::back_inserter (msg.data));

  std::copy ((unsigned char*) &data[0],
             (unsigned char*) &data[0] + data.size(),
             std::back_inserter (msg.data));

  oq->push (msg);
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
  ISMRMRD::EntityHeader e_hdr;
  struct                timeval tv;

  e_hdr.version      = my_version;
  e_hdr.entity_type  = ISMRMRD::ISMRMRD_HANDSHAKE;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream       = 65536;

  std::vector<unsigned char> ent = e_hdr.serialize();

  ISMRMRD::Handshake handshake;
  gettimeofday(&tv, NULL);
  handshake.setTimestamp ((uint64_t)(tv.tv_sec));
  handshake.setClientName (client_name);

  std::cout << "\nPrepairing Handshake:\n";
  std::cout << "Client_name         :" << handshake.getClientName() << "\n";
  std::cout << "Timestamp           :" << handshake.getTimestamp() << "\n";

  std::vector<unsigned char> hand = handshake.serialize();

  uint64_t  size = (uint64_t) (ent.size() + hand.size());
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, hand, out_mq);
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

  e_hdr.version      = my_version;
  e_hdr.entity_type  = ISMRMRD::ISMRMRD_XML_HEADER;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream       = 1;

  std::vector<unsigned char> ent = e_hdr.serialize();
  std::vector<unsigned char> xml (xml_head.begin(), xml_head.end());

  uint64_t size  = (uint64_t) (ent.size() + xml.size());
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, xml, out_mq);
}

/*******************************************************************************
 ******************************************************************************/
void queueCommand (ISMRMRD::Command& cmd, OUTPUT_QUEUE out_mq)
{
  ISMRMRD::EntityHeader e_hdr;

  e_hdr.version      = my_version;
  e_hdr.entity_type  = ISMRMRD::ISMRMRD_COMMAND;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream       = 65537; // TODO: define reserved stream number names

  std::vector<unsigned char> ent     = e_hdr.serialize();
  std::vector<unsigned char> command = cmd.serialize();

  uint64_t size = (uint64_t) (ent.size() + command.size());
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, command, out_mq);
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

  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  queueMessage (size, ent, acquisition, out_mq);
}

/*******************************************************************************
 ******************************************************************************/
void prepareDataQueue
(
  std::string         fname,
  std::string         dname,
  OUTPUT_QUEUE        out_mq,
  std::string         client_name,
  std::atomic<bool>*  done
)
{
  ISMRMRD::Dataset dset (fname.c_str(), dname.c_str());
  std::string xml_head = dset.readHeader();
  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);

  std::cout << "XML Header string size = " << xml_head.size() << " bytes\n";
  queueHandshake (client_name, out_mq);
  queueXmlHeader (xml_head, out_mq);

  int num_acq = dset.getNumberOfAcquisitions (0);
  for (int ii = 0; ii < num_acq; ii++)
  {
    queueAcquisition (dset.readAcquisition<float> (ii, 0), out_mq);
  }
  std::cout << "Queued " << num_acq << " acquisitions\n";

  ISMRMRD::Command cmd;
  cmd.setCommandId (2);
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  queueCommand (cmd, out_mq);

  std::cout << "Finished queueing data and sent STOP_FROM_CLIENT command\n";
  *done = true;
}

/*******************************************************************************
 ******************************************************************************/
int main
(
  int argc,
  char* argv[]
)
{
  std::string       client_name = "Client 1";
  std::string       host        = "127.0.0.1";
  std::string       in_fname    = "testdata.h5";
  std::string       in_dset     = "dataset";
  std::string       out_fname   = "FileOut.h5";
  std::string       out_dset    = "dataset";
  unsigned short    port        = 50050;

  struct timespec   ts;
  ts.tv_sec                     = 0;
  ts.tv_nsec                    = 10000000;


  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h",
     "produce help message")
    ("client name,c",
     po::value<std::string>(&client_name)->default_value(client_name),
     "Client Name")
    ("host,H",
     po::value<std::string>(&host)->default_value(host),
     "Server IP address")
    ("port,p",
     po::value<unsigned short>(&port)->default_value(port),
     "TCP port for server")
    ("fin,i",
     po::value<std::string>(&in_fname)->default_value(in_fname),
     "HDF5 Input file")
    ("in_group,I",
     po::value<std::string>(&in_dset)->default_value(in_dset),
     "Input group name")
    ("fout,o",
     po::value<std::string>(&out_fname)->default_value(out_fname),
     "Output file name")
    ("out_group,O",
     po::value<std::string>(&out_dset)->default_value(out_dset),
     "Output group name")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cout << desc << "\n";
    return 0;
  }

  std::cout << "Using client name <" << client_name << ">, host IP address <"
            << host << ">, and port <" << port << ">" << std::endl;
  std::cout << "Re-start with: icpClient -h to see all options\n" << std::endl;


  // Spawn a thread to prepare the data to be sent to the server
  std::cout << "Getting data from " << in_fname << std::endl;
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
    tcp::endpoint endpoint (boost::asio::ip::address::from_string (host), port);

    boost::system::error_code error = boost::asio::error::host_not_found;
    (*sock).connect(endpoint, error);
    if (error)
    {
      throw boost::system::system_error(error);
    }

    std::thread t2 (readSocket, sock, client_name, out_fname, out_dset);

    nanosleep (&ts, NULL);
    std::cout << "Starting sending data out...\n";

    while (!done || !(*out_mq).empty())
    {
      if (out_mq->empty())
      {
        nanosleep (&ts, NULL);
        continue;
      }

      OUT_MSG msg = out_mq->front();
      boost::asio::write (*sock, boost::asio::buffer (msg.data, 8 + msg.size));
      out_mq->pop();
    }

    std::cout << "Finished sending data out\n";

    t1.join();
    t2.join();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
