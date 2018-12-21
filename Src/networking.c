#include "networking.h"
#include "memory.h"
#include "string.h"
#include "local_time.h"
#include "temperature.h"

static char *buffer;
static char *next_data;
uint16_t remaining_data;

void initialize_sntp(void) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ru.pool.ntp.org");
    sntp_init();
}

static void close_tcp_conn(struct tcp_pcb *pcb) {
      tcp_arg(pcb, NULL);
      tcp_sent(pcb, NULL);
      tcp_recv(pcb, NULL);
      tcp_close(pcb);
}

err_t continue_send_data(void *arg, struct tcp_pcb *pcb, uint16_t len) {
  uint16_t sndbuf_size = tcp_sndbuf(pcb);
  if (sndbuf_size >= remaining_data) {
    tcp_write(pcb, next_data, remaining_data, 0);
    tcp_sent(pcb, NULL);
  } else {
    tcp_write(pcb, next_data, sndbuf_size, TCP_WRITE_FLAG_MORE);
    remaining_data -= sndbuf_size;
    next_data += sndbuf_size;
    tcp_sent(pcb, continue_send_data);
    tcp_output(pcb);
  }
  return ERR_OK;
}

static void send_data (char *data, uint16_t length, struct tcp_pcb *pcb) {
  next_data = data;
  remaining_data = length;
  continue_send_data(data, pcb, 0);
}

static err_t server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
      int len;
      char *pc;

      if (err == ERR_OK && p != NULL) {
            tcp_recved(pcb, p->tot_len);  
            pc = (char *)p->payload;
            len = p->tot_len;
            while (pc[len -1] == '\r' || pc[len-1] == '\n') {
              len--;
              pc[len] = 0;
            }
            
            if (strcmp(pc, "history") == 0) {
              uint16_t offset = 0;
              char *buf = import_csv_history(&offset, &len);
              send_data(buf, len, pcb);
            } 
            
            else if (strcmp(pc, "immediate") == 0) {
              char *buf;
              struct measure_record record;
              perform_calculation(&record);
              buf = stringify_single_record(&record, &len);
              printf("%f\t%f\t%f\n", record.humidity, record.pressure, record.temperature);
              send_data(buf, len, pcb);
            } 
            
            else if (strcmp(pc, "time") == 0) {
              struct tm *ts;
              time_t now = timestamp_conv_local(time(NULL));
              ts = localtime(&now);
              strftime(buffer, MTU_SIZE, "%H:%M:%S %Y-%m-%d\n", ts);
              len = strlen(buffer);
              send_data(buffer, len, pcb);
            } 
            
            else if (strcmp(pc, "csv") == 0) {
              struct measure_record record;
              perform_calculation(&record);
              len = record_to_csv(buffer, &record);
              send_data(buffer, len, pcb);
            }
            pbuf_free(p);
            
      } else {
            pbuf_free(p);
      }
      
      if (err == ERR_OK && p == NULL) {
            close_tcp_conn(pcb);
      }
      return ERR_OK;
}

 static err_t tcp_server_accept (void *arg, struct tcp_pcb *pcb, err_t err) {
      LWIP_UNUSED_ARG(arg);
      LWIP_UNUSED_ARG(err);
      tcp_setprio(pcb, TCP_PRIO_MIN);
      tcp_recv(pcb, server_recv);
      tcp_err(pcb, NULL);
      tcp_poll(pcb, NULL, 4);
      return ERR_OK;
}

void tcp_server_init(void) {
  buffer = allocate_sram_memory(MTU_SIZE);
  struct tcp_pcb *tcp_server_pcb = tcp_new();
  if (tcp_server_pcb != NULL) {
   err_t err; 
   err = tcp_bind(tcp_server_pcb, IP_ADDR_ANY, 127);
   if (err == ERR_OK) {
     tcp_server_pcb = tcp_listen(tcp_server_pcb);
     tcp_accept(tcp_server_pcb, tcp_server_accept); 
   } else { 
     printf("Can not bind pcb\n");
   }
  } else {
   printf("Can not create new pcb\n");
  }
 }