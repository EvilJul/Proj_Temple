#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

typedef struct
{
    char* title;
    int   view, reply, coin, like;
} JSON_CONV_BL_t;

/**
 * @brief      初始化并进行https get请求
 * @param[in]  url  请求的url
 * @return     返回请求的数据
 */
char* http_client_init_get(char* url);

/**
 * @brief Json 格式数据处理(bilibili)
 * @param json_data传入Json字符串
 * @return 返回处理后的数据
 */
JSON_CONV_BL_t bl_json_data_conversion(char* data);

#endif