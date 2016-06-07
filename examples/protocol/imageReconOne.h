#include "imageReconBase.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include <vector>
#include "fftw3.h"

/*******************************************************************************
 ******************************************************************************/
template <typename T>
void circshift(T *out, const T *in, int xdim, int ydim, int xshift, int yshift)
{
  for (int i = 0; i < ydim; i++)
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

class imageReconOne : public imageReconBase
{
  public:

  ISMRMRD::Entity* runReconstruction (std::vector<ISMRMRD::Entity*>& acqs,
                                      ISMRMRD::StorageType           stype,
                                      ISMRMRD::IsmrmrdHeader&        hdr);
  private:

  template <typename S>
  std::vector<ISMRMRD::Acquisition<S> >
    getAcquisitions (std::vector<ISMRMRD::Entity*>& acqs);

  template <typename S>
  ISMRMRD::Entity* reconstruct (std::vector<ISMRMRD::Acquisition<S> >& acqs,
                                ISMRMRD::IsmrmrdHeader& hdr);
};
