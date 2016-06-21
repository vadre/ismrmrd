#ifndef ICPMTQUEUE_H
#define ICPMTQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

#include <iostream>

template <typename T>
class icpMTQueue
{
  public:

  icpMTQueue ()  = default;
  ~icpMTQueue () = default;

  /****************************************************************************/
  void push (const T& data)
  {
    std::unique_lock<std::mutex> lock (_mtx);
    _que.push (data);
    lock.unlock ();
    _cvar.notify_one ();
  }

  /****************************************************************************/
  bool pop (T& data)
  {
    //std::cout << __func__ << ": 1 \n";
    std::unique_lock<std::mutex> lock (_mtx);
    //std::cout << __func__ << ": 2 \n";
  
    while (_que.empty () && !_stop)
    {
    //std::cout << __func__ << ": 3 \n";
      _cvar.wait (lock);
    //std::cout << __func__ << ": 4 \n";
    }
  
    //std::cout << __func__ << ": 5 \n";
    if (_stop)
    {
    //std::cout << __func__ << ": 6 \n";
      _stop = false;
    //std::cout << __func__ << ": 7 \n";
      return false;
    }

    //std::cout << __func__ << ": 8 \n";
    data = _que.front ();
    //std::cout << __func__ << ": 9 \n";
    _que.pop ();
    //std::cout << __func__ << ": 10 \n";
  
    return true;
  }

  /****************************************************************************/
  void stop ()
  {
    _stop = true;
    _cvar.notify_one ();
  }

  /****************************************************************************/
  icpMTQueue (const icpMTQueue&)             = delete;
  icpMTQueue& operator = (const icpMTQueue&) = delete;

  private:

  bool                     _stop;
  std::queue<T>            _que;
  std::mutex               _mtx;
  std::condition_variable  _cvar;
};

#endif //ICPMTQUEUE_H
