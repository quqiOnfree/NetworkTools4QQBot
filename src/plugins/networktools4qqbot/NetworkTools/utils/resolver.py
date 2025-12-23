import dns.resolver


async def resolve_domain(domain: str, record_type: str) -> list:
    """
    解析域名，返回指定类型的 DNS 记录列表
    :param domain: 需要解析的域名
    :param record_type: 记录类型，如 A、AAAA、CNAME 等
    :return: 解析结果列表
    """
    try:
        answers = dns.resolver.query(domain, record_type)
        for rdata in answers:
            return [str(rdata) for rdata in answers]
    except Exception as e:
        return []
