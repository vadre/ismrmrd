#ifndef ICP_CLIENTCALLBACKS_H
#define ICP_CLIENTCALLBACKS_H

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include "icpClient.h"
#include "icpCallback.h"

namespace ISMRMRD { namespace ICP
{
/*******************************************************************************
 ******************************************************************************/
class ClientEntityHandler : public Callback
{
  public:
          ClientEntityHandler (Client*);
          ~ClientEntityHandler () = default;
  void    receive (Callback*, ENTITY*);

  private:

  Client* _client;
};

/*******************************************************************************
 ******************************************************************************/
class ClientImageProcessor : public Callback
{
  public:
       ClientImageProcessor (Client*,
                             std::string fname,
                             std::string dname,
                             std::mutex& mtx);
       ~ClientImageProcessor () = default;
  void receive (Callback*, ENTITY*);

  private:

  template<typename S>
  void writeImage (ISMRMRD::Entity* entity)
  {
    std::cout << "writeImage: 1\t";
    ISMRMRD::Image<S>* img = static_cast<ISMRMRD::Image<S>*>(entity);
    {
    std::cout << "writeImage: 2\t";
      std::lock_guard<std::mutex> guard (_mtx);
    std::cout << "writeImage: 3\t";
      _dset.appendImage (*img, img->getStream());
    std::cout << "writeImage: 4\t";
    }
    std::cout << "writeImage: 5\n";
  }

  Client*             _client;
  ISMRMRD::Dataset       _dset;
  std::mutex&            _mtx;
};

/*******************************************************************************
 ******************************************************************************/

}} //end of namespace scope
#endif // ICP_CLIENTCALLBACKS_H */
