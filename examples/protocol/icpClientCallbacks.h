#ifndef ICP_CLIENTCALLBACKS_H
#define ICP_CLIENTCALLBACKS_H

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include "icpClient.h"
#include "icpCallback.h"

class icpClientEntityHandler : public icpCallback
{
  public:
          icpClientEntityHandler (icpClient*);
          ~icpClientEntityHandler () = default;
  void    receive (icpCallback*, ENTITY*, uint32_t, ETYPE, STYPE, uint32_t);

  private:

  icpClient* _client;
};

class icpClientImageProcessor : public icpCallback
{
  public:
       icpClientImageProcessor (icpClient*,
                                std::string fname,
                                std::string dname);
       ~icpClientImageProcessor () = default;
  void receive (icpCallback*, ENTITY*, uint32_t, ETYPE, STYPE, uint32_t);

  private:

  template<typename S>
  void writeImage (ISMRMRD::Entity* entity)
  {
    ISMRMRD::Image<S>* img = static_cast<ISMRMRD::Image<S>*>(entity);
    _dset.appendImage (*img, ISMRMRD::ISMRMRD_STREAM_IMAGE);
  }

  icpClient*             _client;
  ISMRMRD::Dataset       _dset;
};

#endif // ICP_CLIENTCALLBACKS_H */
