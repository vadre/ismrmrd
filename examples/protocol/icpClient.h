#ifndef ICP_CLIENT_H
#define ICP_CLIENT_H

#include "icpSession.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"

namespace ISMRMRD { namespace ICP
{
// Following default stream numbers are defined in support of option
// where client and server have common default stream number values
// that both client and server are going to use if there was no
// configuration command, and no manifest exchange. This is an
// example of default built-in option, and the following definitions
// could be coming from a commonly available header file, config
// file, or defined as they are here.

const uint32_t DEFAULT_ACQUISITION_SHORT_STREAM  = UNRESERVED_STREAM_START;
const uint32_t DEFAULT_ACQUISITION_INT_STREAM    = UNRESERVED_STREAM_START + 1;
const uint32_t DEFAULT_ACQUISITION_FLOAT_STREAM  = UNRESERVED_STREAM_START + 2;
const uint32_t DEFAULT_ACQUISITION_DOUBLE_STREAM = UNRESERVED_STREAM_START + 3;

const uint32_t DEFAULT_IMAGE_SHORT_STREAM        = UNRESERVED_STREAM_START + 4;
const uint32_t DEFAULT_IMAGE_USHORT_STREAM       = UNRESERVED_STREAM_START + 5;
const uint32_t DEFAULT_IMAGE_INT_STREAM          = UNRESERVED_STREAM_START + 6;
const uint32_t DEFAULT_IMAGE_UINT_STREAM         = UNRESERVED_STREAM_START + 7;
const uint32_t DEFAULT_IMAGE_FLOAT_STREAM        = UNRESERVED_STREAM_START + 8;
const uint32_t DEFAULT_IMAGE_DOUBLE_STREAM       = UNRESERVED_STREAM_START + 9;
const uint32_t DEFAULT_IMAGE_CXFLOAT_STREAM      = UNRESERVED_STREAM_START + 10;
const uint32_t DEFAULT_IMAGE_CXDOUBLE_STREAM     = UNRESERVED_STREAM_START + 11;

/*******************************************************************************
 ******************************************************************************/
class Client
{
  public:

  Client  (SESSION session,
           std::string client_name,
           std::string in_fname,
           std::string out_fname,
           std::string in_dataset,
           std::string out_dataset);
  ~Client ();

  void processHandshake (HANDSHAKE* msg);
  void processCommand   (COMMAND*   msg);
  void processError     (ERRREPORT* msg);
  void taskDone         ();

  private:

  void beginInput (std::mutex&);
  void sendHandshake ();
  void sendCommand (CommandType cmd_type, uint32_t cmd_id);
  void sendError (ErrorType type, std::string descr);
  template <typename S>
  void sendAcquisitions (Dataset& dset, std::mutex& mtx);

  SESSION                _session;
  std::string            _client_name;
  std::string            _in_fname;
  std::string            _out_fname;
  std::string            _in_dset;
  std::string            _out_dset;
  bool                   _server_done;
  bool                   _task_done;
  std::vector<Callback*> _callbacks;
};

}} // end of namespace scope
#endif // ICP_CLIENT_H */
