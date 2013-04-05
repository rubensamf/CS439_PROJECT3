#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void
syscall_handler (struct intr_frame *f UNUSED);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *frame UNUSED) 
{
    //Check Valid esp pointer
    int *sp;
    struct thread *currentThread = thread_current();
    
    if(frame->esp != NULL && is_user_vaddr(frame->esp))
    {
        uint32_t *pd = currentThread->pagedir;  
        if(pagedir_get_page(pd, frame->esp) == NULL)
        {
            currentThread->exit_status = -1;
            thread_exit();
        }
        else
            sp = frame->esp;
    }
    else
    {
        currentThread->exit_status = -1;
        thread_exit();
    }

    
    switch(*sp)
    {     
        /*void halt (void) 
         * Terminates Pintos by calling shutdown_power_off() (declared in "devices/shutdown.h"). 
         * This should be seldom used, because you lose some information about possible deadlock situations, etc. 
         */
        case SYS_HALT:
        {  
            shutdown_power_off();
        }
        
        
        /* void exit (int status)
         *  Terminates the current user program, returning status to the kernel. 
         *  If the process's parent waits for it (see below), this is the status that will be returned. 
         *  Conventionally, a status of 0 indicates success and nonzero values indicate errors. 
         */
        case SYS_EXIT:
        {
                       
         //printf ("%s: exit_status(%d)\n", thread_name(), *(sp+1));
         struct thread *currentThread = thread_current();
         int my_status = *(sp+1);
         
         if(my_status >= INT32_MAX || my_status <= INT32_MIN )
         {
                currentThread->exit_status = -1;
                thread_exit(); 
         }
            
            
          //currentThread->status = my_status;
          currentThread->exit_status = my_status;
          //printf ("%s: exit(%d)\n", thread_name(), thread_current()->status);        
          thread_exit();
        }
        
        case SYS_EXEC:
        {
            /* pid_t exec (const char *cmd_line)
             *  Runs the executable whose name is given in cmd_line, passing any given arguments, 
             *  and returns the new process's program id (pid). Must return pid -1, which otherwise 
             *  should not be a valid pid, if the program cannot load or run for any reason. 
             *  Thus, the parent process cannot return from the exec until it knows whether the child 
             *  process successfully loaded its executable. You must use appropriate synchronization to ensure this. 
             */
          uint32_t* dir= (uint32_t*)sp;
          
          //extract the filename 
          char *file = (char*)dir[1];  
          if(file == NULL || *file == NULL)
            thread_exit();  
            
          printf("(%s) begin",file);  
          frame->eax = process_execute(file);
        }
        
        case SYS_WAIT:
        {
            /* int wait (pid_t pid) */
          frame->eax = process_wait (thread_current()->tid);
          break;
        }
        
        /* bool create (const char *file, unsigned initial_size)
             *  Creates a new file called file initially initial_size bytes in size. 
             *  Returns true if successful, false otherwise. 
             *  Creating a new file does not open it: opening the new file is a separate 
             *  operation which would require a open system call. 
        */
        case SYS_CREATE:
        { 
          uint32_t* dir= (uint32_t*)sp;
          
          //extract the filename 
          char *file_name = (char*)dir[1];  
          
          if(file_name != NULL && is_user_vaddr(file_name))
          {
                struct thread *currentThread = thread_current();
                uint32_t *pd = currentThread->pagedir;  
                if(pagedir_get_page(pd, file_name) == NULL)
                {
                    currentThread->exit_status = -1;
                    thread_exit();
                }
                else
                {
                    if(file_name==NULL|| *file_name==NULL || file_name=="")
                      {
                        thread_current()->exit_status = -1;
                        thread_exit();
                      }

                      //extract file size
                      unsigned initial_size = (unsigned)dir[2];  
                      //printf("File: %s\nSize: %d\n",file,initial_size);


                      printf("(%s) create %s\n",thread_current()->name,file_name);
                      frame->eax = filesys_create(file_name,initial_size);   

                      //printf("SYS_CREATE return value=%d",frame->eax);
                }
            }
            else
          {
                currentThread->exit_status = -1;
                thread_exit();
          }


        }
        
        case SYS_REMOVE:
        { 
            /* bool remove (const char *file)
             *  Deletes the file called file. Returns true if successful, false otherwise. 
             *  A file may be removed regardless of whether it is open or closed, and 
             *  removing an open file does not close it. See Removing an Open File, for details. 
             */
         uint32_t* dir= (uint32_t*)sp;
         
         //extract the filename 
         char *file = (char*)dir[1];
         if(file==NULL||*file==NULL)
            thread_exit();
         
         frame->eax = filesys_remove(file);
         /*if(frame->eax = filesys_remove(file))
             printf("File %s removed correctly.",file);
         else
             printf("File %s was not removed.\n",file);*/
        }
        
        
        
        /*int(fd) open (const char *file) */
        case SYS_OPEN: 
        {          
         uint32_t* dir= (uint32_t*)sp;
         
         //extract the filename 
         char *file_name = (char*)dir[1];
         if(file_name != NULL && is_user_vaddr(file_name))
          {
                struct thread *currentThread = thread_current();
                uint32_t *pd = currentThread->pagedir;  
                if(pagedir_get_page(pd, file_name) == NULL)
                {
                    currentThread->exit_status = -1;
                    thread_exit();
                }
                else
                {
		    
                    if(file_name==NULL|| *file_name==NULL || file_name=="")
                      {
                        thread_current()->exit_status = -1;
                        thread_exit();
                      }

	              //DO WORK
                      struct file *file = filesys_open (file_name);
                      if(file != NULL)
                      {
                          int fd = thread_current()->next_fd;
                          thread_current()->next_fd = thread_current()->next_fd + 1;

                          //Create new FD struct
                          struct file_descriptor *new_file_descriptor;
                          new_file_descriptor->fd = fd;
                          new_file_descriptor->file = file;

                          //Add struct to FD list of current process
                          list_push_front (&thread_current()->new_file_des_list, &new_file_descriptor->elem);


                          frame->eax = fd;
                          printf("OPEN fd value=%d",fd);
                      }
                      else
                        frame->eax = -1;
                }
            }
            else
          {
                currentThread->exit_status = -1;
                thread_exit();
          }
         

        }
        
        /* int filesize (int fd)
         *  Returns the size, in bytes, of the file open as fd. 
         */
        case SYS_FILESIZE: 
        {  
         int fd=*(sp+1);
         
         if(fd >= 0 || fd <= 128)
         {
             struct list_elem *e;
             struct file_descriptor *file_descriptor = NULL;
             for (e = list_begin (&thread_current()->new_file_des_list); e != list_end (&thread_current()->new_file_des_list);
                                    e = list_next (e))
             {
                    file_descriptor = list_entry (e, struct file_descriptor, elem);
                    if(file_descriptor->fd == fd)
                    {
                       frame->eax = file_length(file_descriptor->file);
                       break;
                    }
                    frame->eax = -1;
             }
             
         }
         else
         {
             currentThread->exit_status = -1;
             thread_exit();
         }
        }
        

        
        case SYS_READ: 
        {
            int fd=*(sp+1);
            char *buffer=*(sp+2); 
            unsigned size=*(sp+3);
            struct thread *currentThread = thread_current();
            uint32_t *pd = currentThread->pagedir;  
         
            //Accessing User Memory
             if(!is_user_vaddr (buffer))
             {
                currentThread->exit_status = -1;
                thread_exit();
             }
             if(pagedir_get_page(pd, buffer) == NULL)
             {
               currentThread->exit_status = -1;
               thread_exit();;
             }
         
            frame->eax=-1;
            
            // getiing the fd of specified file     
            struct list_elem *e;
            struct file_descriptor *file_descriptor;
            for (e = list_begin (&thread_current()->new_file_des_list); e != list_end (&thread_current()->new_file_des_list);
                                e = list_next (e))
             {
                       file_descriptor = list_entry (e, struct file_descriptor, elem);
                        if(file_descriptor->fd == fd)
                        {
                            if(file_descriptor->fd == 1)
                               {
                                  if(buffer>=PHYS_BASE)
                                  {
                                      currentThread->exit_status = -1;
                                      thread_exit();
                                  }
                                  if(fd<0||fd>=128)
                                  {
                                      currentThread->exit_status = -1;
                                      thread_exit();
                                  }
                                  if(fd==1)
                                  {
                                      currentThread->exit_status = -1;
                                      thread_exit();
                                  } // reading from STDIN

                               }
                               else
                                  frame->eax=file_read(file_descriptor->file,buffer,(off_t)size);
                                  
                           break;   
                        }
                       frame->eax=-1;
             }
         
        }
        
        /*int write (int fd, const void *buffer, unsigned size) */
        case SYS_WRITE: //9
        {
   
         /*int fd=*(sp+1);
         char *buffer=*(sp+2); 
         unsigned size=*(sp+3);
         struct thread *currentThread = thread_current();
         uint32_t *pd = currentThread->pagedir;  
         
         if(!is_user_vaddr (buffer))
         {
           thread_exit();
         }
         if(pagedir_get_page(pd, buffer) == NULL)
         {
           thread_exit();
         }
         
         
        // getiing the fd of specified file     
        struct list_elem *e;
        struct file_descriptor *file_descriptor;
        for (e = list_begin (&thread_current()->new_file_des_list); e != list_end (&thread_current()->new_file_des_list);
                            e = list_next (e))
         {
              file_descriptor = list_entry (e, struct file_descriptor, elem);
              if(file_descriptor->fd == fd)
              {
                     break;
              }    
         }
        
        //printf("fd=%d\n",file_descriptor->fd);
        if(file_descriptor->file == NULL)
        {
            frame->eax=-1;
        }
        else
        {
           if(file_descriptor->fd == 1)
           {
              if(buffer>=PHYS_BASE)thread_exit();
              if(fd<0||fd>=128){thread_exit();}
              if(fd==0){thread_exit();} // writing to STDIN
              if(fd==1)     //writing to STDOUT
              {
                int a=(int)size;
                while(a>=100)
                {
                  putbuf(buffer,100);
                  buffer=buffer+100;
                  a-=100;
                }
                putbuf(buffer,a);
                frame->eax=(int)size;
              }
           }
             else
             {
                frame->eax=file_write(file_descriptor->file,buffer,(off_t)size);
             }
        }*/
        break;       
        }
        
        /*void seek (int fd, unsigned position) */
        case SYS_SEEK: 
        {
          int fd=*(sp+1);
          unsigned offBy = *(sp+2);
          struct list_elem *e;
          for (e = list_begin (&thread_current()->new_file_des_list);
                e != list_end (&thread_current()->new_file_des_list);
                            e = list_next (e))
             {
                  struct file_descriptor *f = list_entry (e, struct
                  file_descriptor, elem);
                  if(f->fd == fd)
                  {
                     file_seek(&f->file, offBy);
                     break;
                  }
             }
        }
        
        
        case SYS_TELL: //unsigned
        { 
            /*unsigned tell (int fd)
             * Returns the position of the next byte to be read or written in open file fd, 
             * expressed in bytes from the beginning of the file. 
             */
          int fd=*(sp+1);
          struct list_elem *e;
          for (e = list_begin (&thread_current()->new_file_des_list);
                        e != list_end (&thread_current()->new_file_des_list);
                            e = list_next (e))
             {
                  struct file_descriptor *fx = list_entry (e, struct file_descriptor, elem);
                  if(fx->fd == fd)
                  {
                     frame->eax = file_tell(&fx->file);
                     break;
                  }
             }
        }
        
        
        case SYS_CLOSE: //void
        {
            /*void close (int fd)
             * Closes file descriptor fd. Exiting or terminating a process implicitly closes 
             * all its open file descriptors, as if by calling this function for each one. */
                   
             int fd=*(sp+1);      

             struct list_elem *e;

             for (e = list_begin (&thread_current()->new_file_des_list); e != list_end (&thread_current()->new_file_des_list);
                            e = list_next (e))
             {
                  struct file_descriptor *f = list_entry (e, struct file_descriptor, elem);
                  if(f->fd == fd)
                  {
                     list_remove (&f->elem);
                     file_close (&f->file);
                     break;
                  }
             }
        }
    }
    

}