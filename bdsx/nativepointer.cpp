#include "stdafx.h"
#include "nativepointer.h"

#include "encoding.h"

#include <string>
#include <KR3/util/unaligned.h>

using namespace kr;

NativePointer::NativePointer(const JsArguments& args) noexcept
	:JsObjectT(args)
{
}
void NativePointer::initMethods(JsClassT<NativePointer>* cls) noexcept
{
	cls->setMethod(u"move", &NativePointer::move);
	cls->setMethod(u"setAddressPointer", &NativePointer::setAddressPointer);
	cls->setMethod(u"setAddress", &NativePointer::setAddress);
	cls->setMethod(u"setAddressBin", &NativePointer::setAddressBin);
	cls->setMethod(u"setAddressFromBuffer", &NativePointer::setAddressFromBuffer);

	cls->setMethod(u"readBoolean", &NativePointer::readBoolean);
	cls->setMethod(u"readUint8", &NativePointer::readUint8);
	cls->setMethod(u"readUint16", &NativePointer::readUint16);
	cls->setMethod(u"readUint32", &NativePointer::readUint32);
	cls->setMethod(u"readUint64AsFloat", &NativePointer::readUint64AsFloat);
	cls->setMethod(u"readInt8", &NativePointer::readInt8);
	cls->setMethod(u"readInt16", &NativePointer::readInt16);
	cls->setMethod(u"readInt32", &NativePointer::readInt32);
	cls->setMethod(u"readInt64AsFloat", &NativePointer::readInt64AsFloat);
	cls->setMethod(u"readFloat32", &NativePointer::readFloat32);
	cls->setMethod(u"readFloat64", &NativePointer::readFloat64);
	cls->setMethod(u"readPointer", &NativePointer::readPointer);
	cls->setMethod(u"readString", &NativePointer::readString);
	cls->setMethod(u"readBuffer", &NativePointer::readBuffer);
	cls->setMethod(u"readCxxString", &NativePointer::readCxxString);

	cls->setMethod(u"writeBoolean", &NativePointer::writeBoolean);
	cls->setMethod(u"writeUint8", &NativePointer::writeUint8);
	cls->setMethod(u"writeUint16", &NativePointer::writeUint16);
	cls->setMethod(u"writeUint32", &NativePointer::writeUint32);
	cls->setMethod(u"writeUint64WithFloat", &NativePointer::writeUint64WithFloat);
	cls->setMethod(u"writeInt8", &NativePointer::writeInt8);
	cls->setMethod(u"writeInt16", &NativePointer::writeInt16);
	cls->setMethod(u"writeInt32", &NativePointer::writeInt32);
	cls->setMethod(u"writeInt64WithFloat", &NativePointer::writeInt64WithFloat);
	cls->setMethod(u"writeFloat32", &NativePointer::writeFloat32);
	cls->setMethod(u"writeFloat64", &NativePointer::writeFloat64);
	cls->setMethod(u"writePointer", &NativePointer::writePointer);
	cls->setMethod(u"writeString", &NativePointer::writeString);
	cls->setMethod(u"writeBuffer", &NativePointer::writeBuffer);
	cls->setMethod(u"writeCxxString", &NativePointer::writeCxxString);

	cls->setMethod(u"readVarUint", &NativePointer::readVarUint);
	cls->setMethod(u"readVarInt", &NativePointer::readVarInt);
	cls->setMethod(u"readVarBin", &NativePointer::readVarBin);
	cls->setMethod(u"readVarString", &NativePointer::readVarString);
	cls->setMethod(u"readBin", &NativePointer::readBin);
	cls->setMethod(u"readBin64", &NativePointer::readBin64);

	cls->setMethod(u"writeVarUint", &NativePointer::writeVarUint);
	cls->setMethod(u"writeVarInt", &NativePointer::writeVarInt);
	cls->setMethod(u"writeVarBin", &NativePointer::writeVarBin);
	cls->setMethod(u"writeVarString", &NativePointer::writeVarString);
	cls->setMethod(u"writeBin", &NativePointer::writeBin);
}

void NativePointer::move(int32_t lowBits, int32_t highBits) noexcept
{
	m_address += (intptr_t)makeqword(lowBits, highBits);
}
void NativePointer::setAddressPointer(VoidPointer* ptr) noexcept
{
	m_address = (uint8_t*)ptr->getAddressRaw();
}
void NativePointer::setAddress(int32_t lowBits, int32_t highBits) noexcept
{
	m_address = (uint8_t*)makeqword(lowBits, highBits);
}
void NativePointer::setAddressFromBuffer(JsValue buffer) throws(kr::JsException)
{
	Buffer ptr = buffer.getBuffer();
	if (ptr == nullptr) throw JsException(u"argument must be Bufferable");
	m_address = (byte*)ptr.data();
}
void NativePointer::setAddressBin(Text16 text) throws(kr::JsException)
{
	m_address = (byte*)::getBin64(text);
}

bool NativePointer::readBoolean() throws(JsException)
{
	return _readas<bool>();
}
uint8_t NativePointer::readUint8() throws(JsException)
{
	return _readas<uint8_t>();
}
uint16_t NativePointer::readUint16() throws(JsException)
{
	return _readas<uint16_t>();
}
uint32_t NativePointer::readUint32() throws(JsException)
{
	return _readas<int32_t>();
}
double NativePointer::readUint64AsFloat() throws(JsException)
{
	return (double)_readas<uint64_t>();
}
int8_t NativePointer::readInt8() throws(JsException)
{
	return _readas<int8_t>();
}
int16_t NativePointer::readInt16() throws(JsException)
{
	return _readas<int16_t>();
}
int32_t NativePointer::readInt32() throws(JsException)
{
	return _readas<int32_t>();
}
double NativePointer::readInt64AsFloat() throws(JsException)
{
	return (double)_readas<int64_t>();
}
float NativePointer::readFloat32() throws(JsException)
{
	return _readas<float>();
}
double NativePointer::readFloat64() throws(JsException)
{
	return _readas<double>();
}
NativePointer* NativePointer::readPointer() throws(JsException)
{
	NativePointer* pointer = NativePointer::newInstance();
	pointer->m_address = _readas<byte*>();
	return pointer;
}
JsValue NativePointer::readString(JsValue bytes, int encoding) throws(JsException)
{
	if (encoding == ExEncoding::UTF16)
	{
		try
		{
			Text16 text;
			if (bytes.abstractEquals((JsRawData)nullptr))
			{
				char16* end = mem16::find((char16*)m_address, '\0');
				text = Text16((char16*)m_address, end);
				m_address = (byte*)end;
			}
			else
			{
				text = Text16((char16*)m_address, bytes.cast<int>());
				m_address = (byte*)text.end();
			}
			return text;
		}
		catch (...)
		{
			accessViolation(m_address);
		}
	}
	else if (encoding == ExEncoding::BUFFER)
	{
		return readBuffer(bytes.cast<int>());
	}
	else
	{
		try
		{
			TText16 text;
			Text src;
			if (bytes == undefined)
			{
				byte* end = mem::find(m_address, '\0');
				src = Text((char*)m_address, (char*)end);
			}
			else
			{
				src = Text((char*)m_address, bytes.cast<int>());
			}
			m_address = (byte*)src.end();
			Charset cs = (Charset)encoding;
			CHARSET_CONSTLIZE(cs,
				text << (MultiByteToUtf16<cs>)src;
			);
			return text;
		}
		catch (...)
		{
			accessViolation(m_address);
		}
	}
}
JsValue NativePointer::readBuffer(int bytes) throws(JsException)
{
	JsValue value = JsNewTypedArray(JsTypedType::Uint8, bytes);
	try
	{
		value.getBuffer().subcopy(Buffer(m_address, bytes));
		m_address += bytes;
	}
	catch (...)
	{
		accessViolation(m_address);
	}
	return value;
}
TText16 NativePointer::readCxxString(int encoding) throws(JsException)
{
	TText16 text;
	try
	{
		std::string* str = (std::string*)m_address;
		m_address += sizeof(std::string);

		Charset cs = (Charset)encoding;
		CHARSET_CONSTLIZE(cs,
			text << (MultiByteToUtf16<cs>)(Text) * str;
		);
		return text;
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}

void NativePointer::writeBoolean(bool v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeUint8(uint8_t v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeUint16(uint16_t v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeUint32(uint32_t v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeUint64WithFloat(double v) throws(JsException)
{
	_writeas((uint64_t)v);
}
void NativePointer::writeInt8(int8_t v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeInt16(int16_t v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeInt32(int32_t v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeInt64WithFloat(double v) throws(JsException)
{
	_writeas((int64_t)v);
}
void NativePointer::writeFloat32(float v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writeFloat64(double v) throws(JsException)
{
	_writeas(v);
}
void NativePointer::writePointer(StaticPointer* v) throws(JsException)
{
	if (v == nullptr) throw JsException(u"argument must be *Pointer");
	_writeas(v->getAddressRaw());
}
void NativePointer::writeString(JsValue buffer, int encoding) throws(JsException)
{
	if (encoding == ExEncoding::UTF16)
	{
		try
		{
			Text16 text = buffer.cast<Text16>();
			size_t size = text.size();
			memcpy(m_address, text.data(), size);
			m_address += size * sizeof(char16);
		}
		catch (...)
		{
			accessViolation(m_address);
		}
	}
	else if (encoding == ExEncoding::BUFFER)
	{
		writeBuffer(buffer);
	}
	else
	{
		try
		{
			Text16 text = buffer.cast<Text16>();
			TSZ mb;
			Charset cs = (Charset)encoding;
			CHARSET_CONSTLIZE(cs,
				mb << Utf16ToMultiByte<cs>(text);
			);

			size_t size = mb.size();
			memcpy(m_address, mb.data(), size);
			m_address += size;
		}
		catch (...)
		{
			accessViolation(m_address);
		}
	}
}
void NativePointer::writeBuffer(JsValue buffer) throws(JsException)
{
	try
	{
		Buffer buf = buffer.getBuffer();
		if (buf == nullptr) throw JsException(u"argument must be buffer");
		size_t size = buf.size();
		memcpy(m_address, buf.data(), size);
		m_address += size;
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
void NativePointer::writeCxxString(Text16 text, int encoding) throws(JsException)
{
	TSZ mb;
	try
	{
		std::string* str = (std::string*)m_address;
		mb << toUtf8(text);
		Charset cs = (Charset)encoding;
		CHARSET_CONSTLIZE(cs,
			mb << Utf16ToMultiByte<cs>(text);
		);
		str->assign(mb.data(), mb.size());
		m_address += sizeof(std::string);
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}

uint32_t NativePointer::readVarUint() throws(JsException)
{
	try
	{
		uint32_t value = 0;
		for (int i = 0; i <= 28; i += 7) {
			byte b = *m_address++;
			value |= ((b & 0x7f) << i);

			if ((b & 0x80) == 0) {
				return value;
			}
		}
	}
	catch (...)
	{
		accessViolation(m_address);
	}
	throw JsException(u"VarInt did not terminate after 5 bytes!");
}
int32_t NativePointer::readVarInt() throws(JsException)
{
	uint32_t raw = readVarUint();
	return (raw >> 1) ^ -(int32_t)(raw & 1);
}
TText16 NativePointer::readVarBin() throws(JsException)
{
	TText16 out;
	int i = 0;
	uint32_t dest = 0;

	try
	{
		for (;;) {
			byte b = *m_address++;

			dest |= (uint32_t)(b & 0x7f) << i;
			if (i >= 9)
			{
				i -= 9;
				out.write((word)dest);
				dest >>= 16;
				if ((b & 0x80) == 0)
				{
					return std::move(out);
				}
			}
			else
			{
				i += 7;
				if ((b & 0x80) == 0)
				{
					out.write((word)dest);
					return std::move(out);
				}
			}
		}
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
TText16 NativePointer::readVarString(int encoding) throws(JsException)
{
	uint32_t len = readVarUint();
	try
	{
		byte* ptr = m_address;
		m_address += len;
		Charset cs = (Charset)encoding;
		CHARSET_CONSTLIZE(cs,
			return TText16((MultiByteToUtf16<cs>)Text((char*)ptr, len));
		);
		return TText16();
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
JsValue NativePointer::readBin(int words) throws(kr::JsException)
{
	try
	{
		size_t bytes = words * sizeof(char16_t);
		JsValue out = Text16((pcstr16)m_address, (pcstr16)((byte*)m_address + bytes));
		m_address += bytes;
		return out;
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
JsValue NativePointer::readBin64() throws(kr::JsException)
{
	return readBin(4);
}
void NativePointer::writeVarUint(uint32_t value) throws(JsException)
{
	try
	{
		for (;;) {
			if (value <= 0x7f) {
				*m_address++ = value | 0x80;
			}
			else {
				*m_address++ = value & 0x7f;
				return;
			}
			value = (value >> 7);
		}
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
void NativePointer::writeVarInt(int32_t value) throws(JsException)
{
	return writeVarUint((value << 1) ^ (value >> 31));
}
void NativePointer::writeVarBin(Text16 value) throws(JsException)
{
	try
	{
		int v = 0;
		int offset = 0;
		if (value.empty())
		{
			*m_address++ = (byte)0x00;
			return;
		}

		pcstr16 iter = value.begin();
		pcstr16 end = value.end() - 1;
		if (iter < end)
		{
			do
			{
				v |= (word)*iter << offset;
				offset += 16;
				do
				{
					*m_address++ = (byte)(v | 0x80);
					v >>= 7;
					offset -= 7;
				} while (offset > 7);
				iter++;
			} while (iter != end);
		}

		v |= (word)*iter << offset;
		for (;;)
		{
			if (v <= 0x7f)
			{
				*m_address++ = (byte)v;
				return;
			}
			else
			{
				*m_address++ = (byte)(v | 0x80);
			}
			v >>= 7;
		}
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
void NativePointer::writeVarString(Text16 value, int encoding) throws(JsException)
{
	try
	{
		Charset cs = (Charset)encoding;
		CHARSET_CONSTLIZE(cs,
			Utf16ToMultiByte<cs> convert = value;
		size_t size = convert.size();
		writeVarUint(intact<uint32_t>(size));
		convert.copyTo((char*)m_address);
		m_address += size;
		);
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
void NativePointer::writeBin(Text16 value) throws(kr::JsException)
{
	try
	{
		size_t bytes = value.bytes();
		memcpy(m_address, value.data(), bytes);
		m_address += bytes;
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}

template <typename T>
T NativePointer::_readas() throws(JsException)
{
	try
	{
		T value = *(Unaligned<T>*)m_address;
		m_address += sizeof(T);
		return value;
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
template <typename T>
void NativePointer::_writeas(T value) throws(JsException)
{
	try
	{
		*(Unaligned<T>*)m_address = value;
		m_address += sizeof(T);
	}
	catch (...)
	{
		accessViolation(m_address);
	}
}
