#include "stdafx.h"
#include "cachedpdb.h"
#include "nativepointer.h"
#include "jsctx.h"
#include "nodegate.h"

#include <KR3/util/pdb.h>
#include <KR3/data/set.h>
#include <KRWin/handle.h>
#include <KR3/data/crypt.h>

using namespace kr;

CachedPdb g_pdb;
BText<32> g_md5;

kr::BText16<kr::Path::MAX_LEN> CachedPdb::predefinedForCore;

namespace
{
	uint32_t getSelIndex(Text *text) noexcept
	{
		const char* namepos = text->find_r('#');
		if (namepos != nullptr)
		{
			uint32_t value = text->subarr(namepos + 1).to_uint();
			text->cut_self(namepos);
			return value;
		}
		return 0;
	}
	void* getPointer(const JsValue& value) noexcept
	{
		VoidPointer* ptr = value.getNativeObject<VoidPointer>();
		if (ptr == nullptr) return nullptr;
		return ptr->getAddressRaw();
	}
	ATTR_NORETURN void throwAsJsException(FunctionError& err) throws(JsException)
	{
		TSZ16 tsz;
		tsz << (AnsiToUtf16)(Text)err.getFunctionName() << u": failed, ";
		err.getMessageTo(&tsz);
		tsz << u"(0x" << hexf(err.getErrorCode(), 8) << u")\n";
		throw JsException(tsz);
	}

	CriticalSection s_lock;

	struct SymbolMaskInfo
	{
		uint32_t counter;
		uint32_t mask;
	};
	template <bool selfBuffered>
	struct SymbolMap
	{
		Map<Text, SymbolMaskInfo, selfBuffered> targets;
		Keep<io::FOStream<char, false, false>> fos;
		Keep<File> file;
		byte* base;

		SymbolMap(pcstr16 filepath) noexcept
			:fos(nullptr)
		{
			base = (byte*)GetModuleHandleW(nullptr);
			if (filepath != nullptr)
			{
				try
				{
					file = File::openRW(filepath);
				}
				catch (Error&)
				{
				}
			}
		}

		void put(Text tx) noexcept
		{
			const char* namepos = tx.find_r('#');
			uint32_t selbit = 1U << getSelIndex(&tx);

			auto res = targets.insert(tx, { 0, selbit });
			if (!res.second)
			{
				res.first->second.mask |= selbit;
			}
		}

		bool del(Text name) noexcept
		{
			uint32_t selbit = 1U << getSelIndex(&name);
			auto iter = targets.find(name);
			if (iter == targets.end()) return false;

			if ((iter->second.mask & selbit) == 0) return false;
			iter->second.mask &= ~selbit;
			if (iter->second.mask == 0)
			{
				targets.erase(iter);
			}
			return true;
		}

		bool test(Text name, void* address, TText* line, Text* nameWithIndex) noexcept
		{
			auto iter = targets.find(name);
			if (iter == targets.end()) return false;
			SymbolMaskInfo& selects = iter->second;
			uint32_t bit = (1 << selects.counter);
			uint32_t index = selects.counter++;

			if ((selects.mask & bit) == 0) return false;
			selects.mask &= ~bit;
			if (selects.mask == 0)
			{
				targets.erase(iter);
			}

			*line << name;
			if (index != 0)
			{
				*line << '#' << index;
			}
			*nameWithIndex = *line;

			*line << " = 0x" << hexf((byte*)address - base);
			if (fos != nullptr) *fos << "\r\n" << *line;
			return true;
		}

		bool empty() noexcept
		{
			return targets.empty();
		}

		uintptr_t readOffset(io::FIStream<char, false>& fis, Text* name) throws(EofException)
		{
			for (;;)
			{
				Text line = fis.readLine();

				pcstr equal = line.find_r('=');
				if (equal == nullptr) continue;

				*name = line.cut(equal).trim();
				if (!del(*name)) continue;

				Text value = line.subarr(equal + 1).trim();

				if (value.startsWith("0x"))
				{
					return value.subarr(2).to_uintp(16);
				}
				else
				{
					return value.to_uintp();
				}
			}
		}

		bool checkMd5(io::FIStream<char, false>& fis) noexcept
		{
			try
			{
				Text line = fis.readLine();
				if (line != g_md5)
				{
					file->toBegin();
					file->truncate();
					throw EofException();
				}
			}
			catch (EofException&)
			{
				fos = _new io::FOStream<char, false, false>(file);
				*fos << g_md5;
				return true;
			}
			return false;
		}
	};

}

CachedPdb::CachedPdb() noexcept
{
}
CachedPdb::~CachedPdb() noexcept
{
}

void CachedPdb::close() noexcept
{
	if (s_lock.tryEnter())
	{
		m_pdb.close();
		s_lock.leave();
	}
}
int CachedPdb::setOptions(int options) throws(JsException)
{
	if (!s_lock.tryEnter()) throw JsException(u"BUSY. It's using by async task");
	int out = PdbReader::setOptions(options);
	s_lock.leave();
	return out;
}
int CachedPdb::getOptions() throws(JsException)
{
	if (!s_lock.tryEnter()) throw JsException(u"BUSY. It's using by async task");
	int out = PdbReader::getOptions();
	s_lock.leave();
	return out;
}

bool CachedPdb::getProcAddresses(pcstr16 predefined, View<Text> text, Callback cb, void* param, bool quiet) noexcept
{
	CsLock __lock = s_lock;
	SymbolMap<true> targets(predefined);

	for (Text tx : text)
	{
		targets.put(tx);
	}

	if (predefined != nullptr)
	{
		if (targets.file == nullptr)
		{
			if (!quiet) cout << "[BDSX] Failed to open " << (Utf16ToAnsi)(Text16)predefined << endl;
			return false;
		}

		try
		{
			Must<io::FIStream<char, false>> fis = _new io::FIStream<char, false>((File*)targets.file);
			if (targets.checkMd5(*fis))
			{
				if (!quiet) cout << "[BDSX] Generating " << (Utf16ToAnsi)(Text16)predefined << endl;
			}
			else
			{
				for (;;)
				{
					Text name;
					uintptr_t offset = targets.readOffset(*fis, &name);
					cb(name, targets.base + offset, param);
				}
			}
		}
		catch (EofException&)
		{
		}
		if (targets.empty()) return true;
	}

	// load from pdb
	try
	{
		if (!quiet) cout << "[BDSX] PdbReader: Search Symbols..." << endl;
		if (m_pdb.base() == nullptr)
		{
			m_pdb.load();
		}

		struct Local
		{
			SymbolMap<true>& targets;
			void(*cb)(Text name, void* fnptr, void* param);
			void* param;
			bool quiet;
		} local = { targets, cb, param, quiet };

		if (targets.file != nullptr && targets.fos == nullptr) targets.fos = _new io::FOStream<char, false, false>(targets.file);

		m_pdb.search(nullptr, [&local](Text name, void* address, uint32_t typeId) {
			TText line;
			Text nameWithIndex;
			if (!local.targets.test(name, address, &line, &nameWithIndex)) return true;

			if (!local.quiet) cout << line << endl;
			local.cb(nameWithIndex, address, local.param);
			return !local.targets.empty();
			});
		if (targets.fos != nullptr) targets.fos->flush();
	}
	catch (FunctionError& err)
	{
		if (!quiet)
		{
			cerr << err.getFunctionName() << ": failed, ";
			{
				TSZ tsz;
				err.getMessageTo(&tsz);
				cerr << tsz;
			}
			cerr << "(0x" << hexf(err.getErrorCode(), 8) << ')' << endl;
		}
	}

	if (!targets.empty())
	{
		if (!quiet)
		{
			for (auto& item : targets.targets)
			{
				Text name = item.first;
				cerr << name << " not found" << endl;
			}
		}
		return false;
	}

	return true;
}
JsValue CachedPdb::getProcAddresses(pcstr16 predefined, JsValue out, JsValue array, bool quiet) throws(kr::JsException)
{
	if (!s_lock.tryEnter()) throw JsException(u"BUSY. It's using by async task");
	finally { s_lock.leave(); };

	SymbolMap<false> targets(predefined);

	int length = array.getArrayLength();
	for (int i = 0; i < length; i++)
	{
		targets.put(array.get(i).cast<TText>());
	}

	if (predefined != nullptr)
	{
		if (targets.file == nullptr)
		{
			throw JsException(TSZ16() << u"Failed to open " << predefined);
		}
		try
		{
			Must<io::FIStream<char, false>> fis = _new io::FIStream<char, false>((File*)targets.file);
			if (targets.checkMd5(*fis))
			{
				if (!quiet) g_ctx->log(TSZ16() << u"[BDSX] Generating " << predefined);
			}
			else
			{
				for (;;)
				{
					Text name;
					uintptr_t offset = targets.readOffset(*fis, &name);
					NativePointer* ptr = NativePointer::newInstance();
					ptr->setAddressRaw(targets.base + offset);
					out.set(name, ptr);
				}
			}
		}
		catch (EofException&)
		{
		}
		if (targets.empty()) return out;
	}

	// load from pdb
	try
	{
		if (!quiet) g_ctx->log(u"[BDSX] PdbReader: Search Symbols...");
		if (m_pdb.base() == nullptr)
		{
			m_pdb.load();
		}
				
		struct Local
		{
			SymbolMap<false>& targets;
			bool quiet;
			JsValue& out;
		} local = {targets, quiet, out };

		if (targets.file != nullptr && targets.fos == nullptr) targets.fos = _new io::FOStream<char, false, false>(targets.file);

		m_pdb.search(nullptr, [&local](Text name, void* address, uint32_t typeId) {
			TText line;
			Text nameWithIndex;
			if (!local.targets.test(name, address, &line, &nameWithIndex))
			{
				return true;
			}
			if (!local.quiet)
			{
				line << hexf((byte*)address - local.targets.base);
				g_ctx->log(TText16() << (Utf8ToUtf16)line);
			}
			NativePointer* ptr = NativePointer::newInstance();
			ptr->setAddressRaw(address);
			local.out.set(JsValue(nameWithIndex), ptr);
			return !local.targets.empty();
			});
		if (targets.fos != nullptr) targets.fos->flush();
	}
	catch (FunctionError& err)
	{
		throwAsJsException(err);
	}

	if (!targets.empty())
	{
		for (auto& item : targets.targets)
		{
			Text name = item.first;
			if (!quiet) g_ctx->log(TSZ16() << Utf8ToUtf16(name) << u" not found");
		}
	}

	return out;
}

void CachedPdb::search(JsValue masks, JsValue cb) throws(kr::JsException)
{
	try
	{
		if (m_pdb.base() == nullptr)
		{
			m_pdb.load();
		}

		TText filter;
		const char* filterstr = nullptr;
		switch (masks.getType())
		{
		case JsType::String:
			filter = masks.cast<TText>();
			filter << '\0';
			filterstr = filter.data();
			break;
		case JsType::Null:
			break;
		case JsType::Function:
			cb = move(masks);
			break;
		case JsType::Object: {
			// array
			Map<Text, int> finder;
			int length = masks.getArrayLength();
			finder.reserve((size_t)length * 2);

			for (int i = 0; i < length; i++)
			{
				TText text = masks.get(i).cast<TText>();
				finder.insert(text, i);
			}

			m_pdb.search(nullptr, [&](Text name, void* address, uint32_t typeId) {
				auto iter = finder.find(name);
				if (name.endsWith("_fptr"))
				{
					debug();
				}
				if (finder.end() == iter) return true;

				NativePointer* ptr = NativePointer::newInstance();
				ptr->setAddressRaw(address);

				int index = iter->second;
				return cb(masks.get(index), ptr, index).cast<bool>();
				});
			return;
		}
		}
		m_pdb.search(filterstr, [&](Text name, void* address, uint32_t typeId) {
			NativePointer* ptr = NativePointer::newInstance();
			ptr->setAddressRaw(address);
			return cb(TText16() << (Utf8ToUtf16)name, ptr).cast<bool>();
			});
	}
	catch (FunctionError& err)
	{
		throwAsJsException(err);
	}
}
JsValue CachedPdb::getAll(JsValue onprogress) throws(kr::JsException)
{
	try
	{
		bool callback = onprogress.getType() == JsType::Function;
		if (!callback) g_ctx->log(u"[BDSX] PdbReader: Search Symbols...");
		PdbReader reader;
		reader.load();

		struct Local
		{
			JsValue out = JsNewObject;
			timepoint now;
			JsValue onprogress;
			uintptr_t totalcount;
			bool callback;

			void report() noexcept
			{
				if (callback)
				{
					onprogress.call(totalcount);
				}
				else
				{
					TSZ16 tsz;
					tsz << u"[BDSX] PdbReader: Get symbols (" << totalcount << u')';
					g_ctx->log(tsz);
				}
			}
		} local;
		local.now = timepoint::now();
		local.callback = callback;
		local.totalcount = 0;
		if (callback)
		{
			local.onprogress = onprogress;
			local.report();
		}
		reader.getAll([&local](Text name, autoptr address) {
			++local.totalcount;
			timepoint newnow = timepoint::now();
			if (newnow - local.now > 500_ms)
			{
				local.now = newnow;
				local.report();
			}

			NativePointer* ptr = NativePointer::newInstance();
			ptr->setAddressRaw(address);
			local.out.set(name, ptr);
			return true;
			});

		local.report();
		if (!callback)
		{
			g_ctx->log(TSZ16() << u"[BDSX] PdbReader: done (" << local.totalcount << u")");
		}
		return local.out;
	}
	catch (FunctionError& err)
	{
		throwAsJsException(err);
	}
}
JsValue getPdbNamespace() noexcept
{
	JsValue pdb = JsNewObject;
	pdb.set(u"coreCachePath", (Text16)CachedPdb::predefinedForCore);
	pdb.setMethod(u"open", [] {});
	pdb.setMethod(u"close", [] { return g_pdb.close(); });
	pdb.setMethod(u"setOptions", [](int options) { return g_pdb.setOptions(options); });
	pdb.setMethod(u"getOptions", []() { return g_pdb.getOptions(); });
	pdb.setMethod(u"search", [](JsValue masks, JsValue cb) { return g_pdb.search(masks, cb); });
	pdb.setMethod(u"getProcAddresses", [](JsValue out, JsValue array, bool quiet) { return g_pdb.getProcAddresses(CachedPdb::predefinedForCore.data(), out, array, quiet); });

	JsValue getList = JsFunction::makeT([](Text16 predefined, JsValue out, JsValue array, bool quiet) { 
		return g_pdb.getProcAddresses(predefined.data(), out, array, quiet); 
		});
	// should I do this?
	//getList.setMethod(u"async", [](Text16 predefined, JsValue out, JsValue array, bool quiet) { 

	//	Array<Text> list;

	//	AText buffer;
	//	buffer.reserve(1024);

	//	int32_t len = array.getArrayLength();
	//	for (int i = 0; i < len; i++)
	//	{
	//		size_t front = buffer.size();
	//		buffer << (Utf16ToUtf8)array.get(i).cast<Text16>();
	//		size_t back = buffer.size();
	//		buffer << '\0';

	//		list.push(Text((pcstr)front, (pcstr)back));
	//	}

	//	ThreadHandle::createLambda([predefined = AText16::concat(predefined, nullterm), quiet, buffer = move(buffer), list = move(list)]() mutable{
	//		intptr_t off = (intptr_t)buffer.data();
	//		for (Text& v : list)
	//		{
	//			v.addBegin(off);
	//			v.addEnd(off);
	//		}
	//		struct Local
	//		{
	//			AText out;
	//		} local;
	//		local.out.reserve(1024);
	//		g_pdb.getProcAddressesT<Local>(predefined.data(), list, [](Text name, void* fnptr, Local* param) {

	//			}, &local, quiet);
	//		AsyncTask::post([predefined = move(predefined), quiet, buffer = move(buffer), list = move(list)]() mutable{

	//		});
	//		});
	//	});
	pdb.set(u"getList", getList);
	pdb.setMethod(u"getAll", [](JsValue onprogress) { return g_pdb.getAll(onprogress); });
	return pdb;
}
