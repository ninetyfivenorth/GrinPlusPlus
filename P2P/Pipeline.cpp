#include "Pipeline.h"
#include "ConnectionManager.h"

#include <Infrastructure/ThreadManager.h>
#include <Infrastructure/Logger.h>

Pipeline::Pipeline(const Config& config, ConnectionManager& connectionManager, IBlockChainServer& blockChainServer)
	: m_config(config), m_connectionManager(connectionManager), m_blockChainServer(blockChainServer)
{

}

void Pipeline::Start()
{
	m_terminate = false;

	if (m_blockThread.joinable())
	{
		m_blockThread.join();
	}

	m_blockThread = std::thread(Thread_ProcessBlocks, std::ref(*this));

	if (m_transactionThread.joinable())
	{
		m_transactionThread.join();
	}

	m_transactionThread = std::thread(Thread_ProcessTransactions, std::ref(*this));
}

void Pipeline::Stop()
{
	m_terminate = true;

	if (m_blockThread.joinable())
	{
		m_blockThread.join();
	}

	if (m_transactionThread.joinable())
	{
		m_transactionThread.join();
	}
}

void Pipeline::Thread_ProcessBlocks(Pipeline& pipeline)
{
	ThreadManagerAPI::SetCurrentThreadName("BLOCK_PIPE_THREAD");
	LoggerAPI::LogTrace("Pipeline::Thread_ProcessBlocks() - BEGIN");

	while (!pipeline.m_terminate)
	{
		if (pipeline.m_blocksToProcess.size() > 0)
		{
			const BlockEntry& blockEntry = pipeline.m_blocksToProcess.front();
			
			const EBlockChainStatus status = pipeline.m_blockChainServer.AddBlock(blockEntry.block);
			if (status == EBlockChainStatus::INVALID)
			{
				pipeline.m_connectionManager.BanConnection(blockEntry.connectionId, EBanReason::BadBlock);
			}

			std::unique_lock<std::shared_mutex> writeLock(pipeline.m_blockMutex);
			pipeline.m_blocksToProcess.pop_front();
		}
		else if (!pipeline.m_blockChainServer.ProcessNextOrphanBlock())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	}

	LoggerAPI::LogTrace("Pipeline::Thread_ProcessBlocks() - END");
}

bool Pipeline::AddBlockToProcess(const uint64_t connectionId, const FullBlock& block)
{
	if (!IsProcessingBlock(block.GetHash()))
	{
		std::unique_lock<std::shared_mutex> writeLock(m_blockMutex);
		m_blocksToProcess.emplace_back(BlockEntry(connectionId, block));
		return true;
	}

	return false;
}

bool Pipeline::IsProcessingBlock(const Hash& hash) const
{
	std::shared_lock<std::shared_mutex> readLock(m_blockMutex);

	for (auto iter = m_blocksToProcess.cbegin(); iter != m_blocksToProcess.cend(); iter++)
	{
		if (iter->block.GetHash() == hash)
		{
			return true;
		}
	}

	return false;
}

void Pipeline::Thread_ProcessTransactions(Pipeline& pipeline)
{
	ThreadManagerAPI::SetCurrentThreadName("TXN_PIPE_THREAD");
	LoggerAPI::LogTrace("Pipeline::Thread_ProcessTransactions() - BEGIN");

	while (!pipeline.m_terminate)
	{
		if (pipeline.m_transactionsToProcess.size() > 0)
		{
			const TxEntry& txEntry = pipeline.m_transactionsToProcess.front();

			pipeline.m_blockChainServer.AddTransaction(txEntry.transaction, txEntry.poolType);

			std::unique_lock<std::shared_mutex> writeLock(pipeline.m_transactionMutex);
			pipeline.m_transactionsToProcess.pop_front();
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
	}

	LoggerAPI::LogTrace("Pipeline::Thread_ProcessTransactions() - END");
}

bool Pipeline::AddTransactionToProcess(const uint64_t connectionId, const Transaction& transaction, const EPoolType poolType)
{
	if (!IsProcessingTransaction(transaction.GetHash()))
	{
		std::unique_lock<std::shared_mutex> writeLock(m_transactionMutex);
		m_transactionsToProcess.emplace_back(TxEntry(connectionId, transaction, poolType));
		return true;
	}

	return false;
}

bool Pipeline::IsProcessingTransaction(const Hash& hash) const
{
	std::shared_lock<std::shared_mutex> readLock(m_transactionMutex);

	for (auto iter = m_transactionsToProcess.cbegin(); iter != m_transactionsToProcess.cend(); iter++)
	{
		if (iter->transaction.GetHash() == hash)
		{
			return true;
		}
	}

	return false;
}