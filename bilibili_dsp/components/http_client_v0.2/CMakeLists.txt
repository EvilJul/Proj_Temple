set(component_srcs "http_client.c")

idf_component_register(
    SRCS "${component_srcs}" 
    PRIV_REQUIRES   esp_http_client
                    esp_netif
                    # freertos
    INCLUDE_DIRS "."
    EMBED_TXTFILES "bilibili_ca_cert.pem")