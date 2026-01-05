import httpx

async def whois_query(domain: str, raw: bool) -> dict:
    async with httpx.AsyncClient() as client:
        # 设置ua
        headers = {
            "content-type": "application/json",
        }
        if raw:
            raw_sign = 1
        else:
            raw_sign = 0
        response = await client.get(f"https://api.whoiscx.com/whois/?domain={domain}&raw={raw_sign}", headers=headers)
        response.raise_for_status()
        return response.json()
