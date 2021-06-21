#include "Executor.h"

void ExecutorTaskQueue::Push(std::unique_ptr<IExecutorTask> && task)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_queue.emplace_back(std::move(task));
}

std::unique_ptr<IExecutorTask> ExecutorTaskQueue::Pop()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	std::unique_ptr<IExecutorTask> task;

	if (!m_queue.empty())
	{
		task = std::move(m_queue.front());
		m_queue.pop_front();
	}

	return task;
}

std::unique_ptr<IExecutorTask> ExecutorTaskQueue::PopWait(std::condition_variable & cv)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	std::unique_ptr<IExecutorTask> task;

	if (!m_queue.empty())
	{
		task = std::move(m_queue.front());
		m_queue.pop_front();
	}
	else
	{
		cv.wait(lock);
	}

	return task;
}

void Executor::WorkerLoop()
{
	while (m_isRunning)
	{
		std::unique_ptr<IExecutorTask> task = m_workerQueue.PopWait(m_workerCV);
		if (task)
		{
			task->Execute();

			m_completedQueue.Push(std::move(task));
		}
	}
}

Executor::Executor()
{
	m_isRunning = true;

	if (!m_workerThread.joinable())
	{
		m_workerThread = std::thread(&Executor::WorkerLoop, this);
	}
}

Executor::~Executor()
{
	m_isRunning = false;

	if (m_workerThread.joinable())
	{
		m_workerCV.notify_all();
		m_workerThread.join();
	}
}

void Executor::OnUpdate()
{
	std::unique_ptr<IExecutorTask> task = m_completedQueue.Pop();
	if (task)
	{
		task->Callback();
	}
}

void Executor::AddTask(std::unique_ptr<IExecutorTask> && task)
{
	m_workerQueue.Push(std::move(task));
	m_workerCV.notify_one();
}

void Executor::AddTaskCompleted(std::unique_ptr<IExecutorTask> && task)
{
	m_completedQueue.Push(std::move(task));
}
