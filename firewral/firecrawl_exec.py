from annotated_types.test_cases import cases
from dotenv import load_dotenv
from openai import OpenAI
import os, json, requests, time, re

load_dotenv()
if_use_local_model = os.getenv("IF_USE_LOCAL_MODELS")
if bool(int(if_use_local_model)):
    model_name = os.getenv("LOCAL_MODEL_NAME")
    client = OpenAI(
        base_url=os.getenv("LOCAL_MODELS_API"), api_key=os.getenv("LOCAL_MODELS_KEY")
    )
else:
    model_name = os.getenv("API_MODEL_NAME")
    client = OpenAI(
        api_key=os.getenv("SILICONFLOW_API_KEY"), base_url=os.getenv("SSILICONFLOW_API")
    )

urls = []
firecrawl_api = ""


class Colors:
    CYAN = "\033[96m"
    YELLOW = "\033[93m"
    GREEN = "\033[92m"
    RED = "\033[91m"
    MAGENTA = "\033[95m"
    BLUE = "\033[94m"
    RESET = "\033[0m"


def serach_duckduck(query):
    """从duckduck上搜索相关数据"""
    print(f"{Colors.YELLOW}正在从DUCKDUCK进行搜索：{query}...{Colors.RESET}")

    response = requests.get(
        f"https://api.duckduckgo.com/",
        params={"q": query, "format": "json", "no_html": 1, "no_redirect": 1},
    )

    data = response.json()
    results = []

    if data.get("RelatedTopics"):
        for topic in data["RelatedTopics"]:
            if "Text" in topic and "FirstURL" in topic:
                results.append(
                    {
                        "title": topic["Text"],
                        "link": topic["FirstURL"],
                        "snippet": topic.get("Snippet", ""),
                    }
                )

    return results


def select_urls_with_ai(query, serp_results):
    """
    使用大模型筛选检索的URL，返回一个URL列表
    """
    print(f"{Colors.YELLOW}大模型筛选中...{Colors.RESET}")
    try:
        # Prepare the data for R1
        serp_data = [
            {
                "title": r.get("title"),
                "link": r.get("link"),
                "snippet": r.get("snippet"),
            }
            for r in serp_results
            if r.get("link")
        ]

        response = client.chat.completions.create(
            model=f"{model_name}",
            messages=[
                {
                    "role": "system",
                    "content": f"""您是一个URL选择器，始终以有效的JSON格式响应。您从SERP Results结果中选择与{query}结果相关的URL。您的响应必须是一个包含“selected_urls”属性的JSON对象。""",
                },
                {
                    "role": "user",
                    "content": (
                        f"Query: {query}\n"
                        f"SERP Results: {json.dumps(serp_data)}\n\n"
                        "响应必须是一个包含“selected_urls”属性的JSON对象 "
                        'of URLs most likely to help meet the objective. Add a /* to the end of the URL if you think it should search all of the pages in the site. Do not return any social media links. For example: {"selected_urls": ["https://example.com", "https://example2.com"]}'
                    ),
                },
            ],
        )
        print(
            f"{Colors.RED}DEBUG_INFO-->select_urls_with_ai:\n{response}\n{Colors.RESET}"
        )

        try:
            # First try to parse as JSON
            result = json.loads(response.choices[0].message.content)
            if isinstance(result, dict) and "selected_urls" in result:
                urls = result["selected_urls"]
            else:
                # If JSON doesn't have the expected structure, fall back to text parsing
                response_text = response.choices[0].message.content
                urls = [
                    line.strip()
                    for line in response_text.split("\n")
                    if line.strip().startswith(("http://", "https://"))
                ]
        except json.JSONDecodeError:
            # If JSON parsing fails, fall back to text parsing
            response_text = response.choices[0].message.content
            urls = [
                line.strip()
                for line in response_text.split("\n")
                if line.strip().startswith(("http://", "https://"))
            ]

        # Clean up URLs - remove wildcards and trailing slashes
        cleaned_urls = [url.replace("/*", "").rstrip("/") for url in urls]
        cleaned_urls = [url for url in cleaned_urls if url]

        if not cleaned_urls:
            print(f"{Colors.YELLOW}没有找到有效的网址。{Colors.RESET}")
            return []

        print(f"{Colors.CYAN}由大模型筛选出的网址:{Colors.RESET}")
        for url in cleaned_urls:
            print(f"- {url}")

        return cleaned_urls

    except Exception as e:
        print(f"{Colors.RED}出错了: {e}{Colors.RESET}")
        return []


def print_request_info(request):
    """打印请求的完整信息."""
    print("===== 请求信息 =====")
    print("URL:", request.url)  # 打印请求的 URL
    print("Method:", request.method)  # 打印请求方法（GET、POST 等）
    print("Headers:")
    for key, value in request.headers.items():  # 打印所有请求头
        print(f"  {key}: {value}")
    if request.body:  # 打印请求体（如果有）
        print("Body:")
        print(request.body.decode("utf-8"))  # 解码请求体为字符串
    print("====================")


def use_firecrawl(urls, API, use_firecrawl_mode):
    """获取数据中"""
    print(f"{Colors.YELLOW}使用Firecrawl爬取网页内容...{Colors.RESET}")
    headers = {"Content-Type": "application/json"}

    extraction_ids = []
    datas = []
    for url in urls:
        payload = {"url": f"{url}"}

        try:
            session = requests.Session()
            request = requests.Request("POST", API, headers=headers, json=payload)
            prepared_request = session.prepare_request(request)
            print_request_info(prepared_request)

            response = session.send(prepared_request, timeout=30)
            data = response.json()
            print(f"{Colors.RED}DEBUG_INFO:\n{data}\n{Colors.RESET}")

            if not data.get("success"):
                print(
                    f"{Colors.RED}API 错误信息: {data.get('error', 'No error message')}{Colors.RESET}"
                )
                continue

            match use_firecrawl_mode:
                case "1":
                    extraction_id = data.get("id")
                    if not extraction_id:
                        print(f"{Colors.RED}没有从响应中找到ID.{Colors.RESET}")
                        continue
                    extraction_ids.append(extraction_id)

                case "2":
                    md_data = data.get("data")
                    datas.append(md_data.get("markdown"))

        except requests.exceptions.RequestException as e:
            print(f"{Colors.RED}请求失败: {e}{Colors.RESET}")
            continue
        except json.JSONDecodeError as e:
            print(f"{Colors.RED}获取响应失败: {e}{Colors.RESET}")
            continue
        except Exception as e:
            print(f"{Colors.RED}导出数据失败: {e}{Colors.RESET}")
            continue

    if use_firecrawl_mode == "1":
        datas.extend(poll_firecrawl_result(extraction_ids, API))

    return datas


def poll_firecrawl_result(extraction_ids, api, interval=5, max_attempts=12):
    """提取结果"""
    print(f"{Colors.YELLOW}提取数据结果中...{Colors.RESET}")
    datas = []
    for extraction_id in extraction_ids:
        url = f"{api}/{extraction_id}"

        for attempt in range(1, max_attempts + 1):
            response = requests.get(url, timeout=30, verify=False)
            response.raise_for_status()
            data = response.json()
            print(f"{data}")

            if data.get("success") and data.get("data"):
                print(f"{Colors.GREEN}Data successfully extracted:{Colors.RESET}")
                print(json.dumps(data["data"], indent=2))
                datas.append(data["data"])
                break

            elif data.get("success") and not data.get("data"):
                time.sleep(interval)

            else:
                print(
                    f"{Colors.RED}API Error: {data.get('error', 'No error message provided')}{Colors.RESET}"
                )
                break

    return datas


def extract_markdown_from_response(response):
    try:
        # 检查响应是否为空
        if not response:
            print("响应为空")
            return ""

        # 检查响应是否包含id和object字段
        if not response.get("id") or not response.get("object"):
            print("响应中缺少必要的字段")
            print("响应内容:", response)
            return ""

        # 检查响应是否包含choices字段
        choices = response.get("choices", [])
        if not choices:
            print("响应中没有choices")
            return ""

        # 检查第一个一个choice是否包含message字段
        first_choice = choices[0]
        message = first_choice.get("message", {})
        if not message:
            print("choice中没有message")
            print("choice内容:", first_choice)
            return ""

        # 获取消息内容
        message_content = message.get("content", "")
        if not message_content:
            print("消息内容为空")
            print("message内容:", message)
            return ""

        # 打印消息内容以进行调试
        print("原始消息内容:\n", message_content)

        # 去掉三重反引号和前面的换行符
        start_index = message_content.find("```json\n") + len("```json\n")
        end_index = message_content.rfind("\n```")

        if start_index == -1 or end_index == -1:
            print("未找到有效的JSON格式")
            print("消息内容:", message_content)
            return ""

        cleaned_json = message_content[start_index:end_index].strip()
        print(f"{Colors.RED}\n{cleaned_json}\n{Colors.RESET}")

    except Exception as e:
        print(f"{Colors.RED}发生错误：{e}{Colors.RESET}")
        return None

    return cleaned_json


def use_model_clean_datas(datas):
    clean_datas = []

    for data in datas:
        response = client.chat.completions.create(
            model=f"{model_name}",
            messages=[
                {
                    "role": "system",
                    "content": "你是一个文本专业的文本提炼器，可以十分高效的将给你的文本内容提炼出重要内容,你的回答必须是一个包含”markdown“属性的Json对象",
                },
                {
                    "role": "user",
                    "content": f"""
                        以下是我给你的内通：
                        -------------
                        {data}
                        -------------
                        将上述文本内容进行提炼，提取出重要且主要的内容，并以标准的JSON对象返回给我。格式如下：
                        ```json
                        {{"markdown": "提炼后的内容"}}
                        """,
                },
            ],
        )

        try:
            print(f"{Colors.RED}DEBUG_INFO-->clean_datas:\n{response}\n{Colors.RESET}")
            cleaned_json = json.loads(extract_markdown_from_response(response))
            result = cleaned_json.get("markdown")
            if not result:
                print(f"{Colors.RED}清洗数据失败！{Colors.RESET}")
                continue
            clean_datas.append(result)

        except Exception as e:
            print(f"{Colors.RED}调用API时发生错误：{e}{Colors.RESET}")
            continue

    return clean_datas


def is_valid_url(url) -> bool:
    """验证输入的字符串是否为有效的网址"""
    url_regex = re.compile(
        r"^(https?|ftp)://"  # http、https 或 ftp 协议
        r"(?:(?:[A-Z0-9](?:[A-Z0-9-]{0,61}[A-Z0-9])?\.)+(?:[A-Z]{2,6}\.?|[A-Z0-9-]{2,}\.?)|"  # 域名
        r"localhost|"  # 本地主机
        r"\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}|"  # 或 IPv4 地址
        r"\[?[A-F0-9]*:[A-F0-9:]+\]?)"  # 或 IPv6 地址
        r"(?::\d+)?"  # 可选端口
        r"(?:/?|[/?]\S+)$",
        re.IGNORECASE,
    )
    return re.match(url_regex, url) is not None


def main():
    firecrawl_mode_choose = input(
        f"======请选择模式：======\n1、Crawl-深度爬取数据\n2、Scrape-直接爬取网页\n"
    )
    if_use_net_search = input(f"======是否使用网页搜索(y/n)：======\n")
    global urls
    global firecrawl_api

    match if_use_net_search:
        case "y":
            query = input(f"{Colors.BLUE}输入你的问题: {Colors.RESET}")
            serp_results = serach_duckduck(query)
            print(f"{serp_results}")
            if not serp_results:
                return

            urls = select_urls_with_ai(query, serp_results)
            if not urls:
                print(f"{Colors.RED}没有返回任何URL{Colors.RESET}")
                return

        case "n":
            url = input(f"请输入你所要爬取的网址：\n")
            """允许用户输入多个网址，以，分割"""
            if not is_valid_url(url):
                print(f"{Colors.RED}请输入有效网址！{Colors.RESET}\n")
                return None
            urls.append(url)

    match firecrawl_mode_choose:
        case "1":
            firecrawl_api = os.getenv("LOCAL_FIRECRAWL_CRAWL_API")

        case "2":
            firecrawl_api = os.getenv("LOCAL_SCRAPE_EXTRACT_API")

    datas = use_firecrawl(urls, firecrawl_api, firecrawl_mode_choose)
    print(f"{Colors.RED}DEBUG_INFO:\n{datas}\n{Colors.RESET}")
    if datas:
        print(f"{Colors.GREEN}成功导出数据.{Colors.RESET}")
        if_clean_datas = input(f"\n是否需要使用大模型清洗数据(y/n)：\n")

        match if_clean_datas:
            case "y":
                use_model_clean_datas(datas)

            case "n":
                print(f"{Colors.YELLOW}进程结束{Colors.RESET}")

    else:
        print(f"{Colors.RED}导出数据失败！{Colors.RESET}")


if __name__ == "__main__":
    main()
