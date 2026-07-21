// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <vector>

/** 
 * Simple circular buffer container. Container has MaxSize parameter. 
 * Once MaxSize is reached new additions replace the oldest entries (losing data but not exceeding the size).
 */
template <typename T>
class TCircularBuffer
{
public:
	TCircularBuffer(size_t InMaxSize) : MaxSize(InMaxSize) { }
	~TCircularBuffer() = default;
	TCircularBuffer(TCircularBuffer&) = default;
	TCircularBuffer(TCircularBuffer&&) = default;
	TCircularBuffer& operator=(TCircularBuffer&) = default;
	TCircularBuffer& operator=(TCircularBuffer&&) = default;

	const T& operator[](size_t Index) const
	{
		assert(Index < Num());

		size_t EffectiveIndex = StartIndex + Index;
		if (EffectiveIndex >= Buffer.size())
		{
			EffectiveIndex -= Buffer.size();
		}

		assert(EffectiveIndex < Buffer.size());
		return Buffer[EffectiveIndex];
	}

	T& operator[](size_t Index)
	{
		assert(Index < Num());

		size_t EffectiveIndex = StartIndex + Index;
		if (EffectiveIndex >= Buffer.size())
		{
			EffectiveIndex -= Buffer.size();
		}

		assert(EffectiveIndex < Buffer.size());
		return Buffer[EffectiveIndex];
	}

	size_t Num() const
	{
		if (EndIndex > StartIndex)
		{
			assert(Buffer.size() >= (EndIndex - StartIndex));
			return EndIndex - StartIndex;
		}

		assert(Buffer.size() > (StartIndex) || StartIndex == 0);
		assert(Buffer.size() > (EndIndex) || EndIndex == 0);
		return Buffer.size() - StartIndex + EndIndex;
	}

	bool IsEmpty() const
	{
		return StartIndex == EndIndex && Buffer.empty();
	}

	/**
	 * Push back new entry. When buffer is full it will overwrite the oldest entry.
	 * Returns true when existing data is overwritten. O(1) with potential resize.
	 */
	bool PushBack(const T& NewValue)
	{
		T Value = NewValue;
		return PushBack(std::move(Value));
	}

	/** 
	 * Push back new entry. When buffer is full it will overwrite the oldest entry.
	 * Returns true when existing data is overwritten. O(1) with potential resize.
	 */
	bool PushBack(T&& NewValue)
	{
		bool bOverwritten = false;
		size_t InsertionIndex = 0;
		if (Num() == Buffer.size())
		{
			if (Buffer.size() < MaxSize)
			{
				//We haven't reached full capacity yet. Add another entry.
				InsertionIndex = Buffer.size();
				Buffer.resize(Buffer.size() + 1);
			}
			else
			{
				//We are at full capacity. Overwrite the oldest entry.
				assert(StartIndex == EndIndex);
				InsertionIndex = StartIndex;
				++StartIndex;
				if (StartIndex >= Buffer.size())
				{
					StartIndex = 0;
				}

				bOverwritten = true;
			}
		}
		else
		{
			//Use existing spot
			InsertionIndex = EndIndex;
			if (InsertionIndex >= Buffer.size())
			{
				InsertionIndex = 0;
			}
		}

		//Insert data
		std::swap(Buffer[InsertionIndex], NewValue);

		EndIndex = InsertionIndex + 1;
		if (EndIndex >= Buffer.size())
		{
			EndIndex = 0;
		}

		return bOverwritten;
	}

	/** 
	 * Delete the oldest entry. O(1)
	 */
	void PopFront()
	{
		assert(!IsEmpty());
		if (IsEmpty())
		{
			return;
		}

		Buffer[StartIndex++] = T();
		if (StartIndex >= Buffer.size())
		{
			StartIndex = 0;
		}

		//Start reached end which means buffer is empty now.
		if (StartIndex == EndIndex)
		{
			Clear();
		}
	}

	/** 
	 * Insert an entry at specific index. Other entries are moved. O(N)
	 */
	void Insert(size_t InsertionIndex, T&& Value)
	{
		const size_t PrevNum = Num();
		assert(InsertionIndex <= PrevNum);
		if (InsertionIndex > PrevNum)
		{
			return;
		}

		if (InsertionIndex == PrevNum)
		{
			PushBack(Value);
			return;
		}

		size_t EffectiveInsertionIndex = InsertionIndex + StartIndex;
		if (EffectiveInsertionIndex > Buffer.size())
		{
			EffectiveInsertionIndex -= Buffer.size();
		}

		bool bNeedToMoveTillEndIndex = true;

		//Not enough space in buffer
		if (PrevNum == Buffer.size())
		{
			if (Buffer.size() >= MaxSize)
			{
				//Drop last line, then insert
				PopFront();
				--EffectiveInsertionIndex;
			}
			else
			{
				//Increase buffer size as we are not at MaxSize yet
				Buffer.resize(Buffer.size() + 1);
				bNeedToMoveTillEndIndex = false;
			}
		}

		//We have enough space in buffer now
		//Insert

		size_t CurrentIndex = EffectiveInsertionIndex;
		T TempValue = Value;

		while (true)
		{
			std::swap(Buffer[CurrentIndex], TempValue);

			++CurrentIndex;

			if (CurrentIndex == Buffer.size())
			{
				CurrentIndex = 0;

				if (!bNeedToMoveTillEndIndex)
				{
					break;
				}
			}

			if (CurrentIndex == EndIndex)
			{
				Buffer[CurrentIndex] = std::move(TempValue);
				if (++EndIndex >= Buffer.size())
				{
					EndIndex = 0;
				}

				//Done
				break;
			}
		}
	}

	/** 
	 * Erase an entry at certain index moving part of the buffer. O(N)
	 */
	void Erase(size_t EraseIndex)
	{
		const size_t PrevNum = Num();
		assert(EraseIndex < PrevNum);
		if (EraseIndex >= PrevNum)
		{
			return;
		}

		if (EraseIndex == 0)
		{
			PopFront();
			return;
		}

		size_t EffectiveEraseIndex = EraseIndex + StartIndex;
		if (EffectiveEraseIndex >= Buffer.size())
		{
			EffectiveEraseIndex -= Buffer.size();
		}

		size_t CurrentIndex = EffectiveEraseIndex;
		size_t NextIndex = EffectiveEraseIndex + 1;
		if (NextIndex >= Buffer.size())
		{
			//We are at the end
			Buffer.resize(Buffer.size() - 1);
			if (EndIndex >= Buffer.size())
			{
				EndIndex = 0;
			}
			return;
		}

		Buffer[CurrentIndex] = T();

		while (true)
		{
			std::swap(Buffer[CurrentIndex], Buffer[NextIndex]);

			CurrentIndex = NextIndex;
			++NextIndex;

			if (NextIndex == EndIndex)
			{
				--EndIndex;
				break;
			}

			if (NextIndex >= Buffer.size())
			{
				Buffer.resize(Buffer.size() - 1);

				if (EndIndex >= Buffer.size())
				{
					EndIndex = 0;
				}

				if (StartIndex >= Buffer.size())
				{
					StartIndex = 0;
				}

				break;
			}
		}
	}

	void Clear()
	{
		Buffer.clear();
		StartIndex = EndIndex = 0;
	}

	void IncreaseMaxSize(size_t ExtraSize)
	{
		MaxSize += ExtraSize;
	}

private:
	/* Data */
	std::vector<T> Buffer;

	/* The index of the first element */
	size_t StartIndex = 0;

	/* The index of the element after the last one */
	size_t EndIndex = 0;

	/* Maximum buffer size. Once reached new entries will start to overwrite the oldest ones. */
	size_t MaxSize;
};