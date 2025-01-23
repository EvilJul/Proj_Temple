#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

/**
 * @brief      初始化并进行https get请求
 * @param[in]  url  请求的url
 * @return     返回请求的数据
 */
char* http_client_init_get(char* url);

#endif