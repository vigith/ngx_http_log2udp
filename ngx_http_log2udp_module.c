/******************************************/
/* Purpose: Send Request log to UDP	  */
/* Author : Vigith Maurice <v@vigith.com> */
/******************************************/

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
/* UDP packet */
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_LOG2UDP_BUF 4096
#define LOG2UDP_MAX_RETRY 3

/* mq structure for location conf */
typedef struct {
  ngx_flag_t enable;		/* is this module enabled? */
  ngx_str_t mq;			/* server name */
  ngx_uint_t mq_port;		/* server port number */
  ngx_msec_t timeout;		/* how long should we wait before we giveup mq send */
  ngx_uint_t debug;		/* debug value for this module */
  int socketfd;			/* Socket FD */
  struct sockaddr_in servaddr;	/* server addr */
  u_char buf[MAX_LOG2UDP_BUF];	/* one time allocation for logline */
  ngx_conf_t *_cf;
} ngx_http_log2udp_loc_conf_t;

/* http variables */
typedef struct {
  ngx_str_t _name;
  ngx_int_t _index;
} ngx_http_log2udp_var;

static ngx_http_log2udp_var ngx_http_log2udp_vars[] = {
  { ngx_string("remote_user"), -1 },
  { ngx_string("remote_addr"), -1 },
  { ngx_string("http_referer"), -1 },
  { ngx_string("body_bytes_sent"), -1 },
  { ngx_string("request"), -1 },  
  { ngx_string("http_user_agent"), -1 },  
  { ngx_null_string, -1 }
};

static ngx_int_t ngx_http_log2udp_log_init(ngx_conf_t*); /* post configuration for module context */
static ngx_int_t ngx_http_log2udp_log(ngx_http_request_t*); /* log to mq core function */
static char * ngx_http_log2udp_merge_loc_conf(ngx_conf_t*, void*, void*); /* creating the location conf */
static void * ngx_http_log2udp_create_loc_conf(ngx_conf_t*); /* merging location conf with main conf */

static u_char * ngx_log2udp_http_variable(ngx_http_request_t *, u_char *, ngx_int_t);
static u_char * ngx_http_log2udp_status(ngx_http_request_t *, u_char *);
static u_char *ngx_http_log2udp_iso8601(ngx_http_request_t *, u_char *);
static void _join(u_char *, const u_char *, u_char, ngx_http_request_t *); /* creating the location conf */

/* module directives */
static ngx_command_t ngx_http_log2udp_log_commands[] = {
    { 
      ngx_string("log2udp"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_log2udp_loc_conf_t, enable),
      NULL 
    },
    { 
      ngx_string("log2udp_server"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_log2udp_loc_conf_t, mq),
      NULL 
    },
    { 
      ngx_string("log2udp_port"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_log2udp_loc_conf_t, mq_port),
      NULL 
    },
    { 
      ngx_string("log2udp_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_log2udp_loc_conf_t, timeout),
      NULL 
    },
    { 
      ngx_string("log2udp_debug"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_log2udp_loc_conf_t, debug),
      NULL 
    },

    ngx_null_command
};

/* module context */
static ngx_http_module_t ngx_http_log2udp_log_ctx = {
  NULL,				/* preconfiguration */
  ngx_http_log2udp_log_init,	/* postconfiguration */
  NULL,				/* create main configuration */
  NULL,				/* init main configuration */  
  NULL,				/* create server configuration */
  NULL,				/* merge server configuration */
  ngx_http_log2udp_create_loc_conf, /* create location configuration */
  ngx_http_log2udp_merge_loc_conf   /* merge location configuration with location conf */
};

/* module definition */
ngx_module_t  ngx_http_log2udp_module = {
    NGX_MODULE_V1,
    &ngx_http_log2udp_log_ctx,	 /* module context */
    ngx_http_log2udp_log_commands, /* module directives */
    NGX_HTTP_MODULE,		 /* module type */
    NULL,			 /* init master */
    NULL,			 /* init module */
    NULL,			 /* init process */
    NULL,			 /* init thread */
    NULL,			 /* exit thread */
    NULL,			 /* exit process */
    NULL,			 /* exit master */
    NGX_MODULE_V1_PADDING
};

/* create the location conf */
static void *
ngx_http_log2udp_create_loc_conf(ngx_conf_t *cf) {
  printf("create conf\n");
  ngx_http_log2udp_loc_conf_t  *conf;

  /* nginx worries about free'ing */
  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_log2udp_loc_conf_t));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }

  /* UNSET tells the merging functions that these values are to be overridden
     available UNSET options http://lxr.evanmiller.org/http/source/core/ngx_conf_file.h#L56 */
  conf->enable = NGX_CONF_UNSET;
  conf->mq.data = NULL;		/* read the source to find this indirection :-) */
  conf->mq_port = NGX_CONF_UNSET_UINT;
  conf->debug = NGX_CONF_UNSET_UINT;
  conf->timeout = NGX_CONF_UNSET_MSEC;

  return conf;
}

/* merge location conf with server conf */
static char *
ngx_http_log2udp_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    printf("merge conf\n");
    ngx_http_log2udp_loc_conf_t *prev = parent;
    ngx_http_log2udp_loc_conf_t *conf = child;
    ngx_int_t i;
    ngx_http_log2udp_var *v;
    struct timeval timeout;      

    ngx_conf_merge_value(conf->enable, prev->enable, 0); /* default is disabled */
    ngx_conf_merge_str_value(conf->mq, prev->mq, NULL); /* default to NULL and crap out later */
    ngx_conf_merge_uint_value(conf->mq_port, prev->mq_port, 0); /* default to 0 and crap out later */

    ngx_conf_merge_uint_value(conf->debug, prev->debug, 0);	/* disable debug by default */
    ngx_conf_merge_uint_value(conf->timeout, prev->timeout, 5000); /* default is 5ms */

    if (conf->mq.data == NULL || conf->mq_port == 0) {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "Server Address and Port should be valid!");
      return NGX_CONF_ERROR;
    }

    /* UDP Socket Creation */
    conf->socketfd=socket(AF_INET,SOCK_DGRAM,0);
    /* RECV timeout */
    timeout.tv_sec = 0;
    timeout.tv_usec = conf->timeout > 0 ? conf->timeout : 5000; /* make min wait 5000 unless told */
    /* set socket options */
    if (setsockopt (conf->socketfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {		    
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "setsockopt failed!");
      return NGX_CONF_ERROR;
    }

    bzero(&(conf->servaddr),sizeof(conf->servaddr));
    conf->servaddr.sin_family = AF_INET;
    conf->servaddr.sin_addr.s_addr=inet_addr((char *)conf->mq.data); /* we need to typecast, looks like conf->mq.data is unsigned char */
    conf->servaddr.sin_port=htons(conf->mq_port);

    for (v = ngx_http_log2udp_vars; ; v++) {
      /* break on null_string */
      if (v->_name.data == NULL) break;
      i = ngx_http_get_variable_index(cf, &v->_name);
      v->_index = i;
    }

    return NGX_CONF_OK;
}

/* core of log2udp */
static ngx_int_t
ngx_http_log2udp_log(ngx_http_request_t *r)
{
    ngx_http_log2udp_loc_conf_t *log2udp;

    u_char tmp_buf[2048];	/* tmp buffer */
    u_char _buf[2048];		/* tmp buffer */
    u_char c1 = 1;		/* ^A */
    u_char c2 = 2;		/* ^B */
    ngx_http_log2udp_var *v;
    size_t len = 0;		/* strlen */
    ssize_t _s = 0;		/* network send */
    ssize_t _r = 0;		/* network recv */
    ssize_t _r1 = 0;		/* recvfrom return */
    int retry = 0;		/* retry count */

    log2udp = ngx_http_get_module_loc_conf(r, ngx_http_log2udp_module);    

    if (log2udp->enable == 1) {

      /* lets purge it */
      bzero(log2udp->buf, sizeof(log2udp->buf));

      for (v = ngx_http_log2udp_vars; ; v++) {
	/* break on null_string */
	if (v->_name.data == NULL) break;

	bzero(tmp_buf, sizeof(tmp_buf));
	bzero(_buf, sizeof(_buf));
	ngx_log2udp_http_variable(r, tmp_buf, v->_index);
	ngx_sprintf(_buf,"%V%c%s", &v->_name, c2, tmp_buf);
	//ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"there (%i) %s\n", v->_index, _buf);
	_join(log2udp->buf, _buf, c1, r);
      }

      /* status */
      bzero(tmp_buf, sizeof(tmp_buf));
      bzero(_buf, sizeof(_buf));
      ngx_http_log2udp_status(r, tmp_buf);
      ngx_sprintf(_buf,"%s%c%s", "status", c2, tmp_buf);
      _join(log2udp->buf, _buf, c1, r);

      /* log time (iso8601) */
      bzero(tmp_buf, sizeof(tmp_buf));
      bzero(_buf, sizeof(_buf));
      ngx_http_log2udp_iso8601(r, tmp_buf);
      ngx_sprintf(_buf,"%s%c%s", "time_iso8601", c2, tmp_buf);
      _join(log2udp->buf, _buf, c1, r);

      /* UDP Send */
      do {
	len = strlen((char *)log2udp->buf);
	_s = sendto(log2udp->socketfd,log2udp->buf,len,0,
		 (struct sockaddr *)&(log2udp->servaddr),sizeof(log2udp->servaddr));

	bzero(&_r, sizeof(_r));
	_r1 = recvfrom(log2udp->socketfd,&_r,sizeof(_r),0,NULL,NULL);

	if (_r1 == -1 || _s != _r || retry == LOG2UDP_MAX_RETRY) {
	  ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "UDP Send (FAILED) Send (%z bytes) Successful (%z bytes) [Next Retry: %d]", _s, _r, retry + 1);
	}
      } while (retry++ < LOG2UDP_MAX_RETRY && _r != _s);
	

      /* write to error.log, in debug mode */
      if (log2udp->debug)
	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "--- Log2udp# %s", log2udp->buf);
    }

    /* lets clean it up again */
    bzero(log2udp->buf, sizeof(log2udp->buf));

    return NGX_OK;
}

static ngx_int_t
ngx_http_log2udp_log_init (ngx_conf_t *cf)
{
  ngx_uint_t i;
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  for (i = 1; i < cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.nelts; i++) {
    *h = *(h - 1);
    h--;
  }

  *h = ngx_http_log2udp_log;

  return NGX_OK;
}

static void
_join (u_char *s1, const u_char *s2, u_char c, ngx_http_request_t *r) {
  /* don't start with join char */
  if (ngx_strlen(s1) == 0) {
    goto append;
  }

  /* buffer overflow */
  if (ngx_strlen(s1) + ngx_strlen(s2) >= MAX_LOG2UDP_BUF) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "MAX_LOG2UDP_BUF [%d bytes] overflowed! when combining (%s) and (%s)", MAX_LOG2UDP_BUF, s1, s2);
    return;			/* lets not combine this */
  }

  /* skip to right postn */
  while(*s1 != '\0') s1++;

  /* insert the join char */
  *s1++ = c;

 append:
  while ((*s1++ = *s2++) != '\0') ;

  return;
}

/* generic function to access http_variables */
static u_char * 
ngx_log2udp_http_variable(ngx_http_request_t *r, u_char *buf, ngx_int_t index) {
  ngx_http_variable_value_t  *value;
  
  /* get the variable */
  value = ngx_http_get_indexed_variable(r, index);

  if (value == NULL || value->not_found) {
    *buf = '-';
    return buf + 1;
  }

  /* no escaping */
  return ngx_cpymem(buf, value->data, value->len);
}

/* http status */
static u_char *
ngx_http_log2udp_status(ngx_http_request_t *r, u_char *buf) {
  ngx_uint_t  status;

  if (r->err_status) {
    status = r->err_status;
  } else if (r->headers_out.status) {
    status = r->headers_out.status;
  } else if (r->http_version == NGX_HTTP_VERSION_9) {
    *buf++ = '0'; 
    *buf++ = '0'; 
    *buf++ = '9';
    return buf;
  } else {
    status = 0;
  }   

  return ngx_sprintf(buf, "%ui", status);
}

/* log time */
static u_char *
ngx_http_log2udp_iso8601(ngx_http_request_t *r, u_char *buf) {
  return ngx_cpymem(buf, ngx_cached_http_log_iso8601.data,
		    ngx_cached_http_log_iso8601.len);
}

/* log request time (could be used in future) */
/*
static u_char *
ngx_http_log_request_time(ngx_http_request_t *r, u_char *buf) {
  ngx_time_t      *tp;
  ngx_msec_int_t   ms; 

  tp = ngx_timeofday();

  ms = (ngx_msec_int_t)
    ((tp->sec - r->start_sec) * 1000 + (tp->msec - r->start_msec));
  ms = ngx_max(ms, 0); 

  return ngx_sprintf(buf, "%T.%03M", ms / 1000, ms % 1000);
}
*/
