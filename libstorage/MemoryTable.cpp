/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/** @file MemoryTable.cpp
 *  @author ancelmo
 *  @date 20180921
 */
#include "MemoryTable.h"
#include "Common.h"
#include "Table.h"
#include <json/json.h>
#include <libdevcore/easylog.h>
#include <libdevcrypto/Hash.h>
#include <libprecompiled/Common.h>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/lexical_cast.hpp>

using namespace dev;
using namespace dev::storage;
using namespace dev::precompiled;

Entries::Ptr MemoryTable::select(const std::string& key, Condition::Ptr condition) {
	try {
		if(m_remoteDB) {
			//query remoteDB anyway
			Entries::Ptr dbEntries = m_remoteDB->select(m_blockHash, m_blockNum, m_tableInfo->name, key, condition);

			if(!dbEntries) {
				return std::make_shared<Entries>();
			}

			auto entries = std::make_shared<Entries>();
			for(size_t i=0; i<dbEntries->size(); ++i) {
				auto entryIt = m_cache.find(dbEntries->get(i)->getID());
				if(entryIt != m_cache.end()) {
					entries->addEntry(entryIt->second);
				}
				else {
					entries->addEntry(dbEntries->get(i));
				}
			}

			auto indices = processEntries(m_newEntries, condition);
			for(auto it : indices) {
				entries->addEntry(m_newEntries->get(it));
			}

			return entries;
		}

#if 0
		typename Entries::Ptr entries = std::make_shared<Entries>();

		CacheItr it;
		it = m_cache.find(key);
		if (it == m_cache.end()) {
			if (m_remoteDB) {
				entries = m_remoteDB->select(m_blockHash, m_blockNum,
						m_tableInfo->name, key, condition);
				m_cache.insert(std::make_pair(key, entries));
				// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB
				// selects")
				//                    << LOG_KV("key", key) << LOG_KV("records",
				//                    entries->size());
			}
		} else {
			entries = it->second;
		}

		if (!entries) {
			// STORAGE_LOG(DEBUG) << LOG_BADGE("MemoryTable") << LOG_DESC("Can't find data");
			return std::make_shared<Entries>();
		}
		auto indexes = processEntries(entries, condition);
		typename Entries::Ptr resultEntries = std::make_shared<Entries>();
		for (auto& i : indexes) {
			resultEntries->addEntry(entries->get(i));
		}
		return resultEntries;
#endif
	} catch (std::exception& e) {
		 STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("Table select failed for")
		                   << LOG_KV("msg", boost::diagnostic_information(e));
	}


	return std::make_shared<Entries>();
}

int MemoryTable::update(const std::string& key, Entry::Ptr entry, Condition::Ptr condition,
            AccessOptions::Ptr options = std::make_shared<AccessOptions>()) {
	try {
		if (options->check && !checkAuthority(options->origin)) {
			STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("update non-authorized")
			<< LOG_KV("origin", options->origin.hex()) << LOG_KV("key", key);

			return storage::CODE_NO_AUTHORIZED;
		}

		checkField(entry);

		auto entries = select(key, condition);

		std::vector<Change::Record> records;

		for(size_t i=0; i<entries->size(); ++i) {
			auto updateEntry = entries->get(i);

			for (auto& it : *(entry->fields())) {
				records.emplace_back(i, it.first, updateEntry->getField(it.first));
				updateEntry->setField(it.first, it.second);
			}

			//if id equals to zero and not in the m_cache, must be new dirty entry
			if(updateEntry->getID() != 0 && m_cache.find(updateEntry->getID()) == m_cache.end()) {
				m_cache.insert(std::make_pair(updateEntry->getID(), updateEntry));
			}
		}

		return entries->size();
#if 0
		if (options->check && !checkAuthority(options->origin)) {
			// STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("update
			// non-authorized")
			//                     << LOG_KV("origin", options->origin.hex()) << LOG_KV("key",
			//                     key);
			return storage::CODE_NO_AUTHORIZED;
		}
		// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("update") << LOG_KV("key",
		// key);

		typename Entries::Ptr entries = std::make_shared<Entries>();

		CacheItr it;
		{
			// ReadGuard l(x_cache);
			it = m_cache.find(key);
		}
		if (it == m_cache.end()) {
			if (m_remoteDB) {
				entries = m_remoteDB->select(m_blockHash, m_blockNum,
						m_tableInfo->name, key, condition);
				m_cache.insert(std::make_pair(key, entries));
				// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB
				// selects")
				//                    << LOG_KV("key", key) << LOG_KV("records",
				//                    entries->size());
			}
		} else {
			entries = it->second;
		}

		if (!entries) {
			// STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("Can't find data");
			return 0;
		}
		checkField(entry);
		auto indexes = processEntries(entries, condition);
		std::vector<Change::Record> records;

		for (auto& i : indexes) {
			Entry::Ptr updateEntry = entries->get(i);
			for (auto& it : *(entry->fields())) {
				records.emplace_back(i, it.first,
						updateEntry->getField(it.first));
				updateEntry->setField(it.first, it.second);
			}
		}
		this->m_recorder(this->shared_from_this(), Change::Update, key,
				records);

		entries->setDirty(true);

		return indexes.size();
#endif
	} catch (std::exception& e) {
		STORAGE_LOG(ERROR)<< LOG_BADGE("MemoryTable")
		<< LOG_DESC("Access MemoryTable failed for")
		<< LOG_KV("msg", boost::diagnostic_information(e));
	}

	return 0;
}

int MemoryTable::insert(const std::string& key, Entry::Ptr entry,
            AccessOptions::Ptr options = std::make_shared<AccessOptions>(),
            bool needSelect = true) {
	try {
		(void)needSelect;

		if (options->check && !checkAuthority(options->origin)) {
			STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("insert non-authorized")
			<< LOG_KV("origin", options->origin.hex()) << LOG_KV("key", key);
			return storage::CODE_NO_AUTHORIZED;
		}

		checkField(entry);

		Change::Record record(m_newEntries->size() + 1u);
		m_newEntries->addEntry(entry);

		return 1;
#if 0
		if (options->check && !checkAuthority(options->origin)) {
			// STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("insert
			// non-authorized")
			//                     << LOG_KV("origin", options->origin.hex()) << LOG_KV("key",
			//                     key);
			return storage::CODE_NO_AUTHORIZED;
		}
		// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("insert") << LOG_KV("key",
		// key);

		typename Entries::Ptr entries = std::make_shared<Entries>();
		Condition::Ptr condition = std::make_shared<Condition>();

		CacheItr it;
		{
			// ReadGuard l(x_cache);
			it = m_cache.find(key);
		}
		if (it == m_cache.end()) {
			if (m_remoteDB) {
				if (needSelect)
					entries = m_remoteDB->select(m_blockHash, m_blockNum,
							m_tableInfo->name, key, condition);
				else
					entries = std::make_shared<Entries>();

				m_cache.insert(std::make_pair(key, entries));
				// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB
				// selects")
				//                    << LOG_KV("key", key) << LOG_KV("records",
				//                    entries->size());
			}
		} else {
			entries = it->second;
		}
		checkField(entry);
		Change::Record record(entries->size() + 1u);
		std::vector<Change::Record> value { record };
		this->m_recorder(this->shared_from_this(), Change::Insert, key, value);
		if (entries->size() == 0) {
			entries->addEntry(entry);
			{
				// WriteGuard l(x_cache);
				m_cache.insert(std::make_pair(key, entries));
			}
			return 1;
		} else {
			entries->addEntry(entry);
			return 1;
		}
#endif
	} catch (std::exception& e) {
		STORAGE_LOG(ERROR)<< LOG_BADGE("MemoryTable")
		<< LOG_DESC("Access MemoryTable failed for")
		<< LOG_KV("msg", boost::diagnostic_information(e));
	}

	return 0;
}

int MemoryTable::remove(const std::string& key, Condition::Ptr condition,
            AccessOptions::Ptr options = std::make_shared<AccessOptions>()) {
	try {
		if (options->check && !checkAuthority(options->origin)) {
			STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("remove non-authorized")
								<< LOG_KV("origin", options->origin.hex()) << LOG_KV("key", key);
			return storage::CODE_NO_AUTHORIZED;
		}


	} catch (std::exception& e) {
		STORAGE_LOG(ERROR)<< LOG_BADGE("MemoryTable")
		<< LOG_DESC("Access MemoryTable failed for")
		<< LOG_KV("msg", boost::diagnostic_information(e));
	}
#if 0
	if (options->check && !checkAuthority(options->origin)) {
		// STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("remove non-authorized")
		//                     << LOG_KV("origin", options->origin.hex()) << LOG_KV("key", key);
		return storage::CODE_NO_AUTHORIZED;
	}
	// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remove") << LOG_KV("key",
	// key);

	typename Entries::Ptr entries = std::make_shared<Entries>();

	CacheItr it;
	{
		// ReadGuard l(x_cache);
		it = m_cache.find(key);
	}
	if (it == m_cache.end()) {
		if (m_remoteDB) {
			entries = m_remoteDB->select(m_blockHash, m_blockNum,
					m_tableInfo->name, key, condition);
			m_cache.insert(std::make_pair(key, entries));
			// STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB selects")
			//                    << LOG_KV("key", key) << LOG_KV("records", entries->size());
		}
	} else {
		entries = it->second;
	}

	auto indexes = processEntries(entries, condition);

	std::vector<Change::Record> records;
	for (auto& i : indexes) {
		Entry::Ptr removeEntry = entries->get(i);

		removeEntry->setStatus(1);
		records.emplace_back(i);
	}
	this->m_recorder(this->shared_from_this(), Change::Remove, key, records);

	entries->setDirty(true);

	return indexes.size();
#endif
	return 0;
}

dev::h256 MemoryTable::hash() {
	std::map<std::string, Entries::Ptr> tmpMap(m_cache.begin(), m_cache.end());
#if 0
	bytes data;
	for (auto& it : tmpMap)
	{
		if (it.second->dirty())
		{
			// Entries = vector<Entry>
			// LOG(DEBUG) << LOG_BADGE("Report") << LOG_DESC("Entries") << LOG_KV(it.first,
			// "--->");
			data.insert(data.end(), it.first.begin(), it.first.end());
			for (size_t i = 0; i < it.second->size(); ++i)
			{
				if (it.second->get(i)->dirty())
				{
					for (auto& fieldIt : *(it.second->get(i)->fields()))
					{
						// Field
						// LOG(DEBUG) << LOG_BADGE("Report") << LOG_DESC("Field")
						//          << LOG_KV(fieldIt.first, toHex(fieldIt.second));
						if (isHashField(fieldIt.first))
						{
							data.insert(data.end(), fieldIt.first.begin(), fieldIt.first.end());
							data.insert(
									data.end(), fieldIt.second.begin(), fieldIt.second.end());
						}
					}
				}
			}
		}
	}

	if (data.empty())
	{
		return h256();
	}
	bytesConstRef bR(data.data(), data.size());
	h256 hash = dev::sha256(bR);

	return hash;
#endif
	return dev::h256();
}
