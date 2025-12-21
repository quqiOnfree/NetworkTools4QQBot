"""
A Cpp network utils module for python
"""
from __future__ import annotations
import typing
__all__: list[str] = ['ping', 'pingv6', 'tcping', 'tracert']
def ping(arg0: str, arg1: typing.SupportsInt | typing.SupportsIndex, arg2: typing.SupportsInt | typing.SupportsIndex, arg3: typing.SupportsInt | typing.SupportsIndex) -> list:
    """
    ping the destination
    """
def pingv6(arg0: str, arg1: typing.SupportsInt | typing.SupportsIndex, arg2: typing.SupportsInt | typing.SupportsIndex, arg3: typing.SupportsInt | typing.SupportsIndex) -> list:
    """
    ping the destination in ipv6
    """
def tcping(arg0: str, arg1: typing.SupportsInt | typing.SupportsIndex) -> dict:
    """
    tcping a host
    """
def tracert(arg0: str, arg1: typing.SupportsInt | typing.SupportsIndex) -> list:
    """
    tracert the destination
    """
