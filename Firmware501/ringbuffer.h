
#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <libarm.h>

template <typename T, int QUEUE_LENGTH>
class Ringbuffer
{
  private:
    T queue[QUEUE_LENGTH];
    int queue_start;
    int queue_stop;

  public:
    Ringbuffer()
    { 
      queue_start = 0;
      queue_stop = 0;
    }
  
  public:
    int length()
    {
      if (queue_start == queue_stop) return 0;
      if (queue_start < queue_stop) return  (queue_stop - queue_start);
      else return  (QUEUE_LENGTH - queue_start) + queue_stop;
    }

    void clear()
    {
      queue_start = 0;
      queue_stop = 0;
    }

    void push(T c)
    {
    // fprintf(terminal,"push %c qlen: %u qstart: %u qstop: %u \r\n",c,get_queue_length(),queue_start,queue_stop);
      libarm_disable_irq(); 
      queue[queue_stop] = c;
      if (queue_stop == (QUEUE_LENGTH-1)) {
        if (queue_start>0) {
          queue_stop = 0;
        }
      } else {
        if (queue_stop == (queue_start - 1)) {
        } else {
          queue_stop++;
        }
      }
      libarm_enable_irq(); 
    }

    T pop()
    {
      
      T c;
      if (queue_start != queue_stop) {
        libarm_disable_irq(); 
        c = queue[queue_start];
        //  fprintf(terminal,"pop %c qstart: %u qstop: %u \r\n",c,queue_start,queue_stop);
        if (queue_start == (QUEUE_LENGTH-1)) queue_start = 0;
        else {
          queue_start++;
        }
        libarm_enable_irq(); 
      }
      return c;
      
    }

};


#endif
