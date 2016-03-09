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
//const unsigned char stamp[4] = {'I', 'M', 'R', 'D' };

/*******************************************************************************
 ******************************************************************************/
int receive_frame_info (tcp::socket *sock, uint64_t& size, ISMRMRD::EntityHeader& e_hdr)
{
  boost::system::error_code  error;
  std::string temp;

  boost::asio::read (*sock, boost::asio::buffer (&size, sizeof (uint64_t)), error);

  std::cout << "frame_size read status: " << error << std::endl;
  std::cout << "frame_size            : " << size << std::endl;

  boost::asio::read (*sock, boost::asio::buffer (&temp[0],
                     sizeof (ISMRMRD::EntityHeader)), error);
  e_hdr.deserialize (std::vector<unsigned char>(temp.begin(), temp.end()));
  std::cout << "Entity header read status: " << error << std::endl;
  std::cout << "Entity header size       : " << sizeof (e_hdr) << std::endl;
  std::cout << "version                  : " << std::to_string (e_hdr.version) << std::endl;
  std::cout << "entity_type              : " << (ISMRMRD::EntityType) e_hdr.entity_type << std::endl;
  std::cout << "storage_type             : " << (ISMRMRD::StorageType) e_hdr.storage_type << std::endl;
  std::cout << "stream                   : " << e_hdr.stream << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
int receive_handshake (tcp::socket *sock, ISMRMRD::Handshake& handshake)
{
  boost::system::error_code  error;
  uint64_t                   frame_size;
  ISMRMRD::EntityHeader      e_hdr;

  receive_frame_info (sock, frame_size, e_hdr);

  boost::asio::read (*sock, boost::asio::buffer (&handshake, sizeof (ISMRMRD::Handshake)), error);
  std::cout << "Handshake read status: " << error << std::endl;
  std::cout << "Handshake struct size: " << sizeof (ISMRMRD::Handshake) << std::endl;
  std::cout << "Timestamp            :" << handshake.timestamp << std::endl;
  std::cout << "Client name          :" << handshake.client_name << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
int receive_command (tcp::socket *sock, ISMRMRD::Command& cmd)
{
  boost::system::error_code  error;
  uint64_t                   frame_size;
  ISMRMRD::EntityHeader      e_hdr;

  receive_frame_info (sock, frame_size, e_hdr);

  boost::asio::read (*sock, boost::asio::buffer (&cmd, sizeof (ISMRMRD::Command)), error);
  std::cout << "Command read status: " << error << std::endl;
  std::cout << "Command struct size: " << sizeof (ISMRMRD::Command) << std::endl;
  std::cout << "Command            : " << (ISMRMRD::CommandType) cmd.command_type << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
int receive_xml_header (tcp::socket *sock, std::string& xml_head_string)
{
  boost::system::error_code  error;
  uint64_t                   frame_size;
  ISMRMRD::EntityHeader      e_hdr;

  receive_frame_info (sock, frame_size, e_hdr);

  xml_head_string.resize (frame_size - sizeof (ISMRMRD::EntityHeader));
  boost::asio::read (*sock, boost::asio::buffer (&xml_head_string[0], frame_size - sizeof (ISMRMRD::EntityHeader)), error);
  std::cout << "XML Header string read status: " << error << std::endl;
  std::cout << "XML header string size       : " << xml_head_string.size() << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
int receive_image (tcp::socket *sock, ISMRMRD::EntityHeader& e_hdr, std::vector<unsigned char>& img_buf)
{
  boost::system::error_code  error;
  uint64_t                   frame_size;

  receive_frame_info (sock, frame_size, e_hdr);

  img_buf.resize (frame_size - sizeof (ISMRMRD::EntityHeader));
  boost::asio::read (*sock, boost::asio::buffer (img_buf, frame_size - sizeof (ISMRMRD::EntityHeader)), error);
  std::cout << "Image string read status: " << error << std::endl;
  std::cout << "Image string size       : " << img_buf.size() << std::endl;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void socket_read (tcp::socket *sock, std::string my_name, std::string out_fname, std::string dset_name)
{
  std::cout << "\nClient Reader started\n" << std::endl;

  boost::system::error_code  error;
  //uint64_t                   frame_size;
  ISMRMRD::EntityHeader      e_hdr;
  ISMRMRD::Handshake         handshake;
  ISMRMRD::Command           cmd;
  std::string                xml_head_string;
  ISMRMRD::IsmrmrdHeader     xmlHeader;
  std::vector<unsigned char> img_buf;
  

  receive_handshake (sock, handshake);
  if (my_name != handshake.client_name)
  {
    std::cout << "Error: received handshake for different client:" << std::endl;
    std::cout << "My name  :" << my_name << std::endl;
    std::cout << "Intended :" << handshake.client_name << std::endl;
  }

  receive_command   (sock, cmd);
  std::cout << "Received command: " << (ISMRMRD::CommandType) cmd.command_type << std::endl;

  if (cmd.command_type != ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTED)
  {
    std::cout << "The use case for this command does not exist, the received data is discarded." << std::endl;
    return;
  }
  
  // At this point a specific use case might become apparent, but for now we only have a single one to implement
  receive_xml_header (sock, xml_head_string);
  ISMRMRD::deserialize (xml_head_string.c_str(), xmlHeader);
  ISMRMRD::EncodingSpace r_space = xmlHeader.encoding[0].reconSpace;

  receive_image (sock, e_hdr, img_buf);
  ISMRMRD::Image<float> img(r_space.matrixSize.x, r_space.matrixSize.y, // TODO: determine type!
                                         r_space.matrixSize.z,  1); // num channels ???
  img.deserialize (img_buf);

  ISMRMRD::Dataset dset (out_fname.c_str(), dset_name.c_str(), true);
  dset.writeHeader (xml_head_string);
  dset.appendImage("cpp", img); // appendImage is not yet implemented for version 2.x 
                                // consider implementing a version for appending serialized image

  std::cout << "\nFinished processing the data - exiting" << std::endl;
  return;
}

/*******************************************************************************
 ******************************************************************************/
std::string get_handshake_string (std::string client_name)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65536;

  ISMRMRD::Handshake handshake;
  handshake.timestamp = (uint64_t)(tv.tv_sec);
  memset (handshake.client_name, 0, ISMRMRD::Max_Client_Name_Length);
  strncpy (handshake.client_name, client_name.c_str(), client_name.size());

  uint64_t  size = (uint64_t) (sizeof (ISMRMRD::EntityHeader) + sizeof (ISMRMRD::Handshake));
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  std::string result = std::to_string (size);
  result.append ((char*)&(e_hdr.serialize())[0], sizeof (ISMRMRD::EntityHeader));
  result.append ((char*)&(handshake.serialize())[0], sizeof (ISMRMRD::Handshake));

  return result;
}

/*******************************************************************************
 ******************************************************************************/
std::string get_xml_header_string (std::string xml_head)
{
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_XML_HEADER;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 1;

  uint64_t size  = sizeof (ISMRMRD::EntityHeader) + xml_head.size();
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  std::string result = std::to_string (size);
  result.append ((char*)&(e_hdr.serialize())[0], sizeof (ISMRMRD::EntityHeader));
  result.append (xml_head);

  return result;
}

/*******************************************************************************
 ******************************************************************************/
std::string get_command_string (ISMRMRD::CommandType cmd_type)
{
  ISMRMRD::EntityHeader e_hdr;
  e_hdr.version = my_version;
  e_hdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
  e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
  e_hdr.stream = 65537; // Todo: define reserved stream number names

  ISMRMRD::Command cmd;
  cmd.command_type = cmd_type;

  uint64_t size = sizeof (ISMRMRD::EntityHeader) + sizeof (ISMRMRD::Command);
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  std::string result = std::to_string (size);
  result.append ((char*)&(e_hdr.serialize())[0], sizeof (ISMRMRD::EntityHeader));
  result.append ((char*)&(cmd.serialize())[0], sizeof (ISMRMRD::Command));

  return result;
}

/*******************************************************************************
 ******************************************************************************/
//std::string get_acquisition_string (ISMRMRD::Acquisition<T> acq, ISMRMRD::StorageType storage)
template <typename T>
std::string get_acquisition_string (ISMRMRD::Acquisition<T> acq)
{
  ISMRMRD::EntityHeader e_hdr;

  e_hdr.version = my_version;
  e_hdr.entity_type  = ISMRMRD::ISMRMRD_MRACQUISITION;
  e_hdr.storage_type = acq.getStorageType();
  e_hdr.stream       = acq.getStream();

  size_t   acq_size = sizeof (ISMRMRD::AcquisitionHeader) +
                      acq.getNumberOfTrajElements() * sizeof (float) +
                      acq.getNumberOfDataElements() * sizeof (std::complex<T>);

  uint64_t size = sizeof (ISMRMRD::EntityHeader) + acq_size;
  size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

  std::string result = std::to_string (size);
  result.append ((char*)&(e_hdr.serialize())[0], sizeof (ISMRMRD::EntityHeader));
  result.append ((char*)&(acq.serialize())[0], acq_size);

  return result;
}
/*******************************************************************************
 ******************************************************************************/
void prepare_data_queue (std::string fname,
                         std::string dname,
                         std::queue<std::string> *fq,
                         std::string client_name,
                         std::atomic<bool> *done)
{
  ISMRMRD::Dataset dset (fname.c_str(), dname.c_str(), false);
  std::string xml_head = dset.readHeader();

  fq->push (get_handshake_string (client_name));
  fq->push (get_command_string (ISMRMRD::ISMRMRD_COMMAND_IMAGE_RECONSTRUCTION));
  fq->push (get_xml_header_string (xml_head));

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  
  int num_acq = dset.getNumberOfAcquisitions();

  std::cout << "Acq storage type is: " << dset.readAcquisition<float>(0).getStorageType() << std::endl;

  for (int ii = 0; ii < num_acq; ++ii)
  {
    fq->push (get_acquisition_string (dset.readAcquisition<float> (ii)));
  }

  fq->push (get_command_string (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT));

  *done = true;
}

/*******************************************************************************
 ******************************************************************************/
int main (int argc, char* argv[])
{
  std::string       client_name = "HAL 9000";
  std::string       host        = "127.0.0.1";
  std::string       in_fname    = "FileIn.h5";
  std::string       in_dset     = "/dataset";
  std::string       out_fname   = "FileOut.h5";
  std::string       out_dset    = "/dataset";
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
  std::queue<std::string> frame_queue;
  std::thread t1 (prepare_data_queue, in_fname, in_dset, &frame_queue, client_name, &done);

  try
  {
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);
    tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);

    boost::system::error_code error = boost::asio::error::host_not_found;
    socket.connect(endpoint, error);
    if (error) throw boost::system::system_error(error);

    std::thread t2 (socket_read, &socket, client_name, out_fname, out_dset);
    //sleep(1);

    while (!done || !frame_queue.empty())
    {
      if (frame_queue.empty())
      {
        nanosleep (&ts, NULL);
        continue;
      }

      std::string data = frame_queue.front();
      frame_queue.pop();
      boost::asio::write (socket, boost::asio::buffer (data));
      std::cout << client_name << " sent out a frame" << std::endl;
      //boost::asio::write (socket, boost::asio::buffer (data, data.size()));
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
