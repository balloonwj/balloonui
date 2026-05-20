#pragma once

// <msxml2.h> 给出原始 COM 接口声明（IXMLDOMNode / IXMLDOMNodeList /
// IXMLDOMDocument2 / IXMLDOMNamedNodeMap）和 __uuidof(DOMDocument) 需要
// 的 DECLSPEC_UUID 注解。XmlDocument 实现走原始 COM 调用，不用 #import
// 生成的 MSXML2::*Ptr smart pointer 包装。
#include <msxml2.h>
#include <string>

#if defined(UNICODE) || defined(_UNICODE)
	typedef std::wstring tstring;
#else
	typedef std::string tstring;
#endif

class CXmlNodeList;

class CXmlNode
{
public:
	CXmlNode(void);
	~CXmlNode(void);

public:
	BOOL SelectSingleNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL SelectNodes(LPCTSTR pNodeName, CXmlNodeList& objXmlNodeList);
	BOOL GetFirstChildNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL GetNextSiblingNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL GetFirstChildNode(CXmlNode& objXmlNode);
	BOOL GetNextSiblingNode(CXmlNode& objXmlNode);
	std::wstring GetNodeName();
	std::wstring GetText();
	int GetTextInt();
	std::wstring GetAttribute(LPCTSTR lpAttributeName);
	int GetAttributeInt(LPCTSTR lpAttributeName);
	void Release();
	void Attach(IXMLDOMNode* pXMLNode);
	IXMLDOMNode* Detach();

private:
	IXMLDOMNode* m_pXMLNode;
};

class CXmlNodeList
{
public:
	CXmlNodeList(void);
	~CXmlNodeList(void);

public:
	int GetLength();
	BOOL GetItem(int nIndex, CXmlNode& objXmlNode);
	void Release();
	void Attach(IXMLDOMNodeList* pXMLNodeList);
	IXMLDOMNodeList* Detach();

private:
	IXMLDOMNodeList* m_pXMLNodeList;
};

class CXmlDocument
{
public:
	CXmlDocument(void);
	~CXmlDocument(void);

public:
	BOOL Load(LPCTSTR pPath);
	BOOL LoadXml(LPCTSTR pXml);
	BOOL SelectSingleNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL SelectNodes(LPCTSTR pNodeName, CXmlNodeList& objXmlNodeList);
	void Release();

private:
	IXMLDOMDocument2* m_pXMLDoc;
};
