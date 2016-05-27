#ifndef ICP_SESSION_H
#define ICP_SESSION_H

#include <boost/asio.hpp>
#include <thread>
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <map>
#include <time.h>
#include "ismrmrd/version.h"
#include "ismrmrd/xml.h"
#include "icp.h"

/*******************************************************************************
 ******************************************************************************/
class icpSession
{
public:

           icpSession            (SOCKET_PTR sock, uint32_t id);
           ~icpSession           ();
  void     beginReceiving        ();
  void     shutdown              ();
  bool     forwardMessage        (ISMRMRD::EntityType, ISMRMRD::Entity*);
  //template <typename F, typename E, typename S>
  //void     registerHandler       (F func, E etype, S stype);
  template <typename F, typename E>
  void     registerHandler       (F func, E etype);

private:
  SOCKET_PTR                     _sock;
  bool                           _stop;
  uint32_t                       _id;   // debug only
  std::queue<OUT_MSG>            _oq;
  std::thread                    _writer;
  CB_MAP                         _callbacks;

  bool     getMessage            ();
  uint32_t receiveFrameInfo      (IN_MSG& in_msg);
  void     deliver               (IN_MSG& in_msg);
  void     transmit              ();
  void     queueMessage          (std::vector<unsigned char>& ent,
                                  std::vector<unsigned char>& data);
  template<typename ...A>
  void     call                  (uint32_t index, A&& ... args);
  
};

/*******************************************************************************
 * Template
 ******************************************************************************/
template <typename F, typename E>
void icpSession::registerHandler
(
  F func,
  E etype
)
{
  std::unique_ptr<F> f_uptr (new F (func));
  _callbacks.insert (CB_MAP::value_type (etype, std::move (f_uptr)));

  return;
}
/*
template <typename F, typename E, typename S>
void icpServer::registerHandler
(
  F func,
  E etype,
  S stype
)
{
  std::unique_ptr<F> f_uptr (new F (func));
  uint32_t index = etype * 100 + stype; //TODO: ISMRMRD::StorageType <= 99
  _callbacks.insert (CB_MAP::value_type (index, std::move (f_uptr)));

  return;
}
*/

#endif // ICP_SESSION_H
