#include "Command/EditorCommandHistory.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition)
		{
			std::cout << "[PASS] " << name << '\n';
			return;
		}

		std::cerr << "[FAIL] " << name << '\n';
		++g_failures;
	}

	class RecordingCommand final : public IEditorCommand
	{
	public:
		RecordingCommand(
			std::string name,
			std::vector<std::string>& log,
			bool executeResult = true,
			bool undoResult = true)
			: m_name(std::move(name))
			, m_log(log)
			, m_executeResult(executeResult)
			, m_undoResult(undoResult)
		{
		}

		bool Execute() override
		{
			m_log.push_back("Execute " + m_name);
			return m_executeResult;
		}

		bool Undo() override
		{
			m_log.push_back("Undo " + m_name);
			return m_undoResult;
		}

	private:
		std::string m_name;
		std::vector<std::string>& m_log;
		bool m_executeResult;
		bool m_undoResult;
	};

	void TestExecuteUndoRedo()
	{
		EditorCommandHistory history;
		std::vector<std::string> log;

		Check(history.Execute(std::make_unique<RecordingCommand>("A", log)),
			"Execute accepts a successful command");
		Check(history.GetUndoCount() == 1 && history.GetRedoCount() == 0,
			"Execute puts the command on the undo stack");
		Check(history.Undo(), "Undo succeeds when history exists");
		Check(history.GetUndoCount() == 0 && history.GetRedoCount() == 1,
			"Undo moves the command to the redo stack");
		Check(history.Redo(), "Redo succeeds after Undo");
		Check(history.GetUndoCount() == 1 && history.GetRedoCount() == 0,
			"Redo moves the command back to the undo stack");
		Check(log == std::vector<std::string>{"Execute A", "Undo A", "Execute A"},
			"Redo re-executes the same command");
	}

	void TestLastInFirstOutOrder()
	{
		EditorCommandHistory history;
		std::vector<std::string> log;
		history.Execute(std::make_unique<RecordingCommand>("A", log));
		history.Execute(std::make_unique<RecordingCommand>("B", log));
		history.Undo();
		history.Undo();
		history.Redo();
		history.Redo();

		Check(log == std::vector<std::string>{
			"Execute A", "Execute B", "Undo B", "Undo A", "Execute A", "Execute B"},
			"Undo and Redo process commands in history order");
	}

	void TestNewCommandClearsRedoBranch()
	{
		EditorCommandHistory history;
		std::vector<std::string> log;
		history.Execute(std::make_unique<RecordingCommand>("A", log));
		history.Execute(std::make_unique<RecordingCommand>("B", log));
		history.Undo();
		Check(history.CanRedo(), "Undo creates a redo branch");

		history.Execute(std::make_unique<RecordingCommand>("C", log));
		Check(!history.CanRedo() && history.GetRedoCount() == 0,
			"A new command clears the old redo branch");
		Check(!history.Redo(), "Cleared redo history cannot be replayed");
	}

	void TestFailuresPreserveHistory()
	{
		std::vector<std::string> log;

		EditorCommandHistory executeFailureHistory;
		Check(!executeFailureHistory.Execute(
			std::make_unique<RecordingCommand>("FailExecute", log, false, true)),
			"Failed Execute is reported");
		Check(!executeFailureHistory.CanUndo() && !executeFailureHistory.CanRedo(),
			"Failed Execute is not recorded");

		EditorCommandHistory undoFailureHistory;
		undoFailureHistory.Execute(
			std::make_unique<RecordingCommand>("FailUndo", log, true, false));
		Check(!undoFailureHistory.Undo(), "Failed Undo is reported");
		Check(undoFailureHistory.GetUndoCount() == 1 &&
			undoFailureHistory.GetRedoCount() == 0,
			"Failed Undo leaves the command on the undo stack");

		EditorCommandHistory redoFailureHistory;
		// The history API cannot naturally create a command whose first Execute succeeds
		// and whose second Execute fails, so Redo failure is covered by a stateful command.
		class FailOnSecondExecute final : public IEditorCommand
		{
		public:
			bool Execute() override { return ++executeCount == 1; }
			bool Undo() override { return true; }
			int executeCount = 0;
		};
		redoFailureHistory.Execute(std::make_unique<FailOnSecondExecute>());
		redoFailureHistory.Undo();
		Check(!redoFailureHistory.Redo(), "Failed Redo is reported");
		Check(redoFailureHistory.GetUndoCount() == 0 &&
			redoFailureHistory.GetRedoCount() == 1,
			"Failed Redo leaves the command on the redo stack");
	}

	void TestEmptyNullAndClear()
	{
		EditorCommandHistory history;
		std::vector<std::string> log;
		Check(!history.Undo() && !history.Redo(),
			"Empty history rejects Undo and Redo");
		Check(!history.Execute(nullptr), "Null command is rejected");

		history.Execute(std::make_unique<RecordingCommand>("A", log));
		history.Execute(std::make_unique<RecordingCommand>("B", log));
		history.Undo();
		history.Clear();
		Check(!history.CanUndo() && !history.CanRedo(),
			"Clear removes both undo and redo history");
	}
}

int main()
{
	TestExecuteUndoRedo();
	TestLastInFirstOutOrder();
	TestNewCommandClearsRedoBranch();
	TestFailuresPreserveHistory();
	TestEmptyNullAndClear();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " EditorCommandHistory test(s) failed.\n";
		return 1;
	}

	std::cout << "All EditorCommandHistory tests passed.\n";
	return 0;
}
