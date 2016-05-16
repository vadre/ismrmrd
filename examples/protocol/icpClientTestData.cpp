#include "icpClientTestData.h"

namespace ICPCLIENTTESTDATA
{
/*****************************************************************************
 ****************************************************************************/
void icpClientTestData::setServerDone ()
{
  _server_done = true;
}

/*****************************************************************************
 ****************************************************************************/
bool icpClientTestData::isServerDone ()
{
  return _server_done;
}

/*****************************************************************************
 ****************************************************************************/
void icpClientTestData::setSessionClosed ()
{
  _session_closed = true;
}

/*****************************************************************************
 ****************************************************************************/
bool icpClientTestData::isSessionClosed ()
{
  return _session_closed;
}

/*******************************************************************************
 ******************************************************************************/
bool icpClientTestData::isIsmrmrdHeaderSet()
{
  return _ismrmrd_header_received;
}

/*******************************************************************************
 ******************************************************************************/
void icpClientTestData::setClientName (std::string name)
{
  _client_name = name;
}

/*******************************************************************************
 ******************************************************************************/
std::string  icpClientTestData::getClientName ()
{
  return _client_name;
}

/*******************************************************************************
 ******************************************************************************/
bool icpClientTestData::addIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader hdr
)
{
  _ismrmrd_header = hdr;
  _ismrmrd_header_received = true;
  return _ismrmrd_header_received;
}

/*****************************************************************************
 ****************************************************************************/
ISMRMRD::IsmrmrdHeader icpClientTestData::getIsmrmrdHeader ()
{
  if (!_ismrmrd_header_received)
  {
    throw std::runtime_error ("IsmrmrdHeader is not yet available");
  }
  return _ismrmrd_header;
}

/*****************************************************************************
 ****************************************************************************/
std::string icpClientTestData::getIsmrmrdHeaderSerialized ()
{
  if (!_ismrmrd_header_received)
  {
    throw std::runtime_error ("IsmrmrdHeader is not yet available");
  }
  std::stringstream sstr;
  ISMRMRD::serialize (_ismrmrd_header, sstr);
  std::string xml_head = sstr.str();

  return xml_head;
}

/*****************************************************************************
 ****************************************************************************/
void icpClientTestData::setInputFileName (std::string fname)
{
  _input_file_name = fname;
}

/*****************************************************************************
 ****************************************************************************/
std::string icpClientTestData::getInputFileName ()
{
  return _input_file_name;
}

/*****************************************************************************
 ****************************************************************************/
void icpClientTestData::setOutputFileName (std::string fname)
{
  _output_file_name = fname;
}

/*****************************************************************************
 ****************************************************************************/
std::string icpClientTestData::getOutputFileName ()
{
  return _output_file_name;
}

/*****************************************************************************
 ****************************************************************************/
void icpClientTestData::setInputDataset (std::string dset_name)
{
  _input_dset_name = dset_name;
}

/*****************************************************************************
 ****************************************************************************/
std::string icpClientTestData::getInputDataset ()
{
  return _input_dset_name;
}

/*****************************************************************************
 ****************************************************************************/
void icpClientTestData::setOutputDataset (std::string dset_name)
{
  _output_dset_name = dset_name;
}

/*****************************************************************************
 ****************************************************************************/
std::string icpClientTestData::getOutputDataset ()
{
  return _output_dset_name;
}

/*****************************************************************************
 ****************************************************************************/
icpClientTestData::icpClientTestData()
: _server_done (false),
  _session_closed (false),
  _ismrmrd_header_received (false)
{}

/*****************************************************************************
 ****************************************************************************/

} /* namespace ICPCLIENTTESTDATA */
