#include "Data/PMMO_ReactionData.h"

const FPMMO_ReactionDataEntry& UPMMO_ReactionData::FindReactionChecked(FGameplayTag ReactionTag) const
{
	const FPMMO_ReactionDataEntry* Reaction = FindReactionPtr(ReactionTag);
	check(Reaction);
	return *Reaction;
}

bool UPMMO_ReactionData::FindReaction(FGameplayTag ReactionTag, FPMMO_ReactionDataEntry& OutReaction) const
{
	const FPMMO_ReactionDataEntry* Reaction = FindReactionPtr(ReactionTag);
	if (!Reaction) return false;

	OutReaction = *Reaction;
	return true;
}

const FPMMO_ReactionDataEntry* UPMMO_ReactionData::FindReactionPtr(FGameplayTag ReactionTag) const
{
	if (!ReactionTag.IsValid()) return nullptr;

	for (const FPMMO_ReactionDataEntry& Entry : Reactions)
	{
		if (Entry.ReactionTag.IsValid() && ReactionTag.MatchesTag(Entry.ReactionTag))
		{
			return &Entry;
		}
	}

	return nullptr;
}
