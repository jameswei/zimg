/*   
 *   zimg - high performance image storage and processing system.
 *       http://zimg.buaa.us 
 *   
 *   Copyright (c) 2013, Peter Zhao <zp@buaa.us>.
 *   All rights reserved.
 *   
 *   Use and distribution licensed under the BSD license.
 *   See the LICENSE file for full text.
 * 
 */


/**
 * @file zhttpd.c
 * @brief http protocol parse functions.
 * @author 招牌疯子 zp@buaa.us
 * @version 1.0
 * @date 2013-07-19
 */

#include "zhttpd.h"
#include "zimg.h"
#include "zutil.h"
#include "zlog.h"

char uri_root[512];

static const char * guess_type(const char *type);
static const char * guess_content_type(const char *path);
static int print_headers(evhtp_header_t * header, void * arg); 
void dump_request_cb(evhtp_request_t *req, void *arg);
void echo_cb(evhtp_request_t *req, void *arg);
void post_request_cb(evhtp_request_t *req, void *arg);
void send_document_cb(evhtp_request_t *req, void *arg);

static const struct table_entry {
	const char *extension;
	const char *content_type;
} content_type_table[] = {
	{ "txt", "text/plain" },
	{ "c", "text/plain" },
	{ "h", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/htm" },
	{ "css", "text/css" },
	{ "gif", "image/gif" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "png", "image/png" },
	{ "pdf", "application/pdf" },
	{ "ps", "application/postsript" },
	{ NULL, NULL },
};

static const char * method_strmap[] = {
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "DELETE",
    "MKCOL",
    "COPY",
    "MOVE",
    "OPTIONS",
    "PROPFIND",
    "PROPATCH",
    "LOCK",
    "UNLOCK",
    "TRACE",
    "CONNECT",
    "PATCH",
    "UNKNOWN",
};

/**
 * @brief guess_type It returns a HTTP type by guessing the file type.
 *
 * @param type Input a file type.
 *
 * @return Const string of type.
 */
static const char * guess_type(const char *type)
{
	const struct table_entry *ent;
	for (ent = &content_type_table[0]; ent->extension; ++ent) {
		if (!evutil_ascii_strcasecmp(ent->extension, type))
			return ent->content_type;
	}
	return "application/misc";
}

/* Try to guess a good content-type for 'path' */
/**
 * @brief guess_content_type Likes guess_type, but it process a whole path of file.
 *
 * @param path The path of a file you want to guess type.
 *
 * @return The string of type.
 */
static const char * guess_content_type(const char *path)
{
	const char *last_period, *extension;
	const struct table_entry *ent;
	last_period = strrchr(path, '.');
	if (!last_period || strchr(last_period, '/'))
		goto not_found; /* no exension */
	extension = last_period + 1;
	for (ent = &content_type_table[0]; ent->extension; ++ent) {
		if (!evutil_ascii_strcasecmp(ent->extension, extension))
			return ent->content_type;
	}

not_found:
	return "application/misc";
}

/**
 * @brief print_headers It displays all headers and values.
 *
 * @param header The header of a request.
 * @param arg The evbuff you want to store the k-v string.
 *
 * @return It always return 1 for success.
 */
static int print_headers(evhtp_header_t * header, void * arg) 
{
    evbuf_t * buf = arg;

    evbuffer_add(buf, header->key, header->klen);
    evbuffer_add(buf, ": ", 2);
    evbuffer_add(buf, header->val, header->vlen);
    evbuffer_add(buf, "\r\n", 2);
	return 1;
}

/**
 * @brief dump_request_cb The callback of a dump request.
 *
 * @param req The request you want to dump.
 * @param arg It is not useful.
 */
void dump_request_cb(evhtp_request_t *req, void *arg)
{
    const char *uri = req->uri->path->full;

	//switch (evhtp_request_t_get_command(req)) {
    int req_method = evhtp_request_get_method(req);
    if(req_method >= 16)
        req_method = 16;

	LOG_PRINT(LOG_INFO, "Received a %s request for %s", method_strmap[req_method], uri);
    evbuffer_add_printf(req->buffer_out, "uri : %s\r\n", uri);
    evbuffer_add_printf(req->buffer_out, "query : %s\r\n", req->uri->query_raw);
    evhtp_headers_for_each(req->uri->query, print_headers, req->buffer_out);
    evbuffer_add_printf(req->buffer_out, "Method : %s\n", method_strmap[req_method]);
    evhtp_headers_for_each(req->headers_in, print_headers, req->buffer_out);

	evbuf_t *buf = req->buffer_in;;
	puts("Input data: <<<");
	while (evbuffer_get_length(buf)) {
		int n;
		char cbuf[128];
		n = evbuffer_remove(buf, cbuf, sizeof(buf)-1);
		if (n > 0)
			(void) fwrite(cbuf, 1, n, stdout);
	}
	puts(">>>");

    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/plain", 0, 0));
    evhtp_send_reply(req, EVHTP_RES_OK);
}

/**
 * @brief echo_cb Callback function of echo test.
 *
 * @param req The request of a test url.
 * @param arg It is not useful.
 */
void echo_cb(evhtp_request_t *req, void *arg)
{
    evbuffer_add_printf(req->buffer_out, "<html><body><h1>zimg works!</h1></body></html>");
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
    evhtp_send_reply(req, EVHTP_RES_OK);
}


/**
 * @brief post_request_cb The callback function of a POST request to upload a image.
 *
 * @param req The request with image buffer.
 * @param arg It is not useful.
 */
void post_request_cb(evhtp_request_t *req, void *arg)
{
    int post_size = 0;
    char *boundary = NULL, *boundary_end = NULL;
    int boundary_len = 0;
    char *fileName = NULL;
    char *boundaryPattern = NULL;
    char *buff = NULL;

    int req_method = evhtp_request_get_method(req);
    if(req_method >= 16)
        req_method = 16;
    LOG_PRINT(LOG_INFO, "Method: %d", req_method);
    if(strcmp(method_strmap[req_method], "POST") != 0)
    {
        LOG_PRINT(LOG_INFO, "Request Method Not Support.");
        goto err;
    }


    const char *content_len = evhtp_header_find(req->headers_in, "Content-Length");
    post_size = atoi(content_len);
    const char *content_type = evhtp_header_find(req->headers_in, "Content-Type");
    if(strstr(content_type, "multipart/form-data") == 0)
    {
        LOG_PRINT(LOG_ERROR, "POST form error!");
        goto err;
    }
    else if(strstr(content_type, "boundary") == 0)
    {
        LOG_PRINT(LOG_ERROR, "boundary NOT found!");
        goto err;
    }

    boundary = strchr(content_type, '=');
    boundary++;
    boundary_len = strlen(boundary);

    if (boundary[0] == '"') 
    {
        boundary++;
        boundary_end = strchr(boundary, '"');
        if (!boundary_end) 
        {
            LOG_PRINT(LOG_ERROR, "Invalid boundary in multipart/form-data POST data");
            goto err;
        }
    } 
    else 
    {
        /* search for the end of the boundary */
        boundary_end = strpbrk(boundary, ",;");
    }
    if (boundary_end) 
    {
        boundary_end[0] = '\0';
        boundary_len = boundary_end-boundary;
    }

    LOG_PRINT(LOG_INFO, "boundary Find. boundary = %s", boundary);
            
    /* 依靠evbuffer自己实现php处理函数 */
	evbuf_t *buf;
    buf = req->buffer_in;
    buff = (char *)malloc(post_size);
    int rmblen, evblen;
    int img_size = 0;

    if(evbuffer_get_length(buf) <= 0)
    {
        LOG_PRINT(LOG_ERROR, "Empty Request!");
        goto err;
    }

    while((evblen = evbuffer_get_length(buf)) > 0)
    {
        LOG_PRINT(LOG_INFO, "evblen = %d", evblen);
        rmblen = evbuffer_remove(buf, buff, evblen);
        LOG_PRINT(LOG_INFO, "rmblen = %d", rmblen);
        if(rmblen < 0)
        {
            LOG_PRINT(LOG_ERROR, "evbuffer_remove failed!");
            goto err;
        }
    }

    int start = -1, end = -1;
    const char *fileNamePattern = "filename=";
    const char *typePattern = "Content-Type";
    const char *quotePattern = "\"";
    const char *blankPattern = "\r\n";
    boundaryPattern = (char *)malloc(boundary_len + 4);
    sprintf(boundaryPattern, "\r\n--%s", boundary);
    LOG_PRINT(LOG_INFO, "boundaryPattern = %s, strlen = %d", boundaryPattern, (int)strlen(boundaryPattern));
    if((start = kmp(buff, post_size, fileNamePattern, strlen(fileNamePattern))) == -1)
    {
        LOG_PRINT(LOG_ERROR, "Content-Disposition Not Found!");
        goto err;
    }
    start += 9;
    if(buff[start] == '\"')
    {
        start++;
        if((end = kmp(buff+start, post_size-start, quotePattern, strlen(quotePattern))) == -1)
        {
            LOG_PRINT(LOG_ERROR, "quote \" Not Found!");
            goto err;
        }
    }
    else
    {
        if((end = kmp(buff+start, post_size-start, blankPattern, strlen(blankPattern))) == -1)
        {
            LOG_PRINT(LOG_ERROR, "quote \" Not Found!");
            goto err;
        }
    }
    fileName = (char *)malloc(end + 1);
    memcpy(fileName, buff+start, end);
    fileName[end] = '\0';
    LOG_PRINT(LOG_INFO, "fileName = %s", fileName);

    char fileType[32];
    if(get_type(fileName, fileType) == -1)
    {
        LOG_PRINT(LOG_ERROR, "Get Type of File[%s] Failed!", fileName);
        goto err;
    }
    if(is_img(fileType) != 1)
    {
        LOG_PRINT(LOG_ERROR, "fileType[%s] is Not Supported!", fileType);
        goto err;
    }

    end += start;

    if((start = kmp(buff+end, post_size-end, typePattern, strlen(typePattern))) == -1)
    {
        LOG_PRINT(LOG_ERROR, "Content-Type Not Found!");
        goto err;
    }
    start += end;
    LOG_PRINT(LOG_INFO, "start = %d", start);
    if((end =  kmp(buff+start, post_size-start, blankPattern, strlen(blankPattern))) == -1)
    {
        LOG_PRINT(LOG_ERROR, "Image Not complete!");
        goto err;
    }
    end += start;
    LOG_PRINT(LOG_INFO, "end = %d", end);
    start = end + 4;
    LOG_PRINT(LOG_INFO, "start = %d", start);
    if((end = kmp(buff+start, post_size-start, boundaryPattern, strlen(boundaryPattern))) == -1)
    {
        LOG_PRINT(LOG_ERROR, "Image Not complete!");
        goto err;
    }
    end += start;
    LOG_PRINT(LOG_INFO, "end = %d", end);
    img_size = end - start;


    LOG_PRINT(LOG_INFO, "post_size = %d", post_size);
    LOG_PRINT(LOG_INFO, "img_size = %d", img_size);
    if(img_size <= 0)
    {
        LOG_PRINT(LOG_ERROR, "Image Size is Zero!");
        goto err;
    }

    char md5sum[33];

    LOG_PRINT(LOG_INFO, "Begin to Save Image...");
    if(save_img(buff+start, img_size, md5sum) == -1)
    {
        LOG_PRINT(LOG_ERROR, "Image Save Failed!");
        goto err;
    }

    evbuffer_add_printf(req->buffer_out, 
        "<html>\n<head>\n"
        "<title>Upload Successfully</title>\n"
        "</head>\n"
        "<body>\n"
        "<h1>MD5: %s</h1>\n"
        "</body>\n</html>\n",
        md5sum
        );
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_INFO, "============post_request_cb() DONE!===============");
    goto done;

err:
    evbuffer_add_printf(req->buffer_out, "<html><body><h1>Upload Failed!</h1></body><html>"); 
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
    evhtp_send_reply(req, EVHTP_RES_200);
    LOG_PRINT(LOG_INFO, "============post_request_cb() ERROR!===============");

done:
    if(fileName)
        free(fileName);
    if(boundaryPattern)
        free(boundaryPattern);
    if(buff)
        free(buff);
}


/**
 * @brief send_document_cb The callback function of get a image request.
 *
 * @param req The request with a list of params and the md5 of image.
 * @param arg It is not useful.
 */
void send_document_cb(evhtp_request_t *req, void *arg)
{
    char *md5 = NULL;
	size_t len;
    zimg_req_t *zimg_req = NULL;
	char *buff = NULL;

    int req_method = evhtp_request_get_method(req);
    if(req_method >= 16)
        req_method = 16;
    LOG_PRINT(LOG_INFO, "Method: %d", req_method);
    if(strcmp(method_strmap[req_method], "POST") == 0)
    {
        LOG_PRINT(LOG_INFO, "POST Request.");
        post_request_cb(req, NULL);
        return;
    }
	else if(strcmp(method_strmap[req_method], "GET") != 0)
    {
        LOG_PRINT(LOG_INFO, "Request Method Not Support.");
        goto err;
    }


	const char *uri;
	uri = req->uri->path->full;
	const char *rfull = req->uri->path->full;
	const char *rpath = req->uri->path->path;
	const char *rfile= req->uri->path->file;
	LOG_PRINT(LOG_INFO, "uri->path->full: %s",  rfull);
	LOG_PRINT(LOG_INFO, "uri->path->path: %s",  rpath);
	LOG_PRINT(LOG_INFO, "uri->path->file: %s",  rfile);

    if(strstr(uri, "favicon.ico"))
    {
        LOG_PRINT(LOG_INFO, "favicon.ico Request, Denied.");
        return;
    }
	LOG_PRINT(LOG_INFO, "Got a GET request for <%s>",  uri);

	/* Don't allow any ".."s in the path, to avoid exposing stuff outside
	 * of the docroot.  This test is both overzealous and underzealous:
	 * it forbids aceptable paths like "/this/one..here", but it doesn't
	 * do anything to prevent symlink following." */
	if (strstr(uri, ".."))
		goto err;

    md5 = (char *)malloc(strlen(uri) + 1);
    if(uri[0] == '/')
        strcpy(md5, uri+1);
    else
        strcpy(md5, uri);
	LOG_PRINT(LOG_INFO, "md5 of request is <%s>",  md5);
    if(is_md5(md5) == -1)
    {
        LOG_PRINT(LOG_WARNING, "Url is Not a zimg Request.");
        goto err;
    }
	/* This holds the content we're sending. */

    int width, height, proportion, gray;
    evhtp_kvs_t *params;
    params = req->uri->query;
    if(!params)
    {
        width = 0;
        height = 0;
        proportion = 1;
        gray = 0;
    }
    else
    {
        const char *str_w, *str_h;
        str_w = evhtp_kv_find(params, "w");
        if(str_w == NULL)
            str_w = "0";
        str_h = evhtp_kv_find(params, "h");
        if(str_h == NULL)
            str_h = "0";
        LOG_PRINT(LOG_INFO, "w() = %s; h() = %s;", str_w, str_h);
        if(strcmp(str_w, "g") == 0 && strcmp(str_h, "w") == 0)
        {
            LOG_PRINT(LOG_INFO, "Love is Eternal.");
            evbuffer_add_printf(req->buffer_out, "<html>\n <head>\n"
                "  <title>Love is Eternal</title>\n"
                " </head>\n"
                " <body>\n"
                "  <h1>Single1024</h1>\n"
                "Since 2008-12-22, there left no room in my heart for another one.</br>\n"
                "</body>\n</html>\n"
                );
            evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
            evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
            evhtp_send_reply(req, EVHTP_RES_OK);
            LOG_PRINT(LOG_INFO, "============send_document_cb() DONE!===============");
            goto done;
        }
        else
        {
            width = atoi(str_w);
            height = atoi(str_h);
            const char *str_p = evhtp_kv_find(params, "p");
            const char *str_g = evhtp_kv_find(params, "g");
            if(str_p)
                proportion = atoi(str_p);
            else
                proportion = 1;
            if(str_g)
                gray = atoi(str_g);
            else
                gray = 0;
        }
    }

    zimg_req = (zimg_req_t *)malloc(sizeof(zimg_req_t)); 
    zimg_req -> md5 = md5;
    zimg_req -> width = width;
    zimg_req -> height = height;
    zimg_req -> proportion = proportion;
    zimg_req -> gray = gray;

    int get_img_rst = get_img(zimg_req, &buff,  &len);


    if(get_img_rst == -1)
    {
        LOG_PRINT(LOG_ERROR, "zimg Requset Get Image[MD5: %s] Failed!", zimg_req->md5);
        goto err;
    }

    LOG_PRINT(LOG_INFO, "get buffer length: %d", len);
    evbuffer_add(req->buffer_out, buff, len);

    LOG_PRINT(LOG_INFO, "Got the File!");
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "image/jpeg", 0, 0));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_INFO, "============send_document_cb() DONE!===============");


    if(get_img_rst == 2)
    {
        if(new_img(buff, len, zimg_req->rsp_path) == -1)
        {
            LOG_PRINT(LOG_WARNING, "New Image[%s] Save Failed!", zimg_req->rsp_path);
        }
    }
    goto done;

err:
    evbuffer_add_printf(req->buffer_out, "<html><body><h1>404 Not Found!</h1></body></html>");
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", "zimg/1.0.0 (Unix) (OpenSUSE/Linux)", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
    evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
    LOG_PRINT(LOG_INFO, "============send_document_cb() ERROR!===============");

done:
	if(buff)
		free(buff);
    if(zimg_req)
    {
        if(zimg_req->md5)
            free(zimg_req->md5);
        if(zimg_req->rsp_path)
            free(zimg_req->rsp_path);
        free(zimg_req);
    }
}

