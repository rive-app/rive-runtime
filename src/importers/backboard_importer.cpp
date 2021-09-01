
#include "rive/importers/backboard_importer.hpp"
#include "rive/nested_artboard.hpp"

using namespace rive;

BackboardImporter::BackboardImporter(Backboard* backboard) :
    m_Backboard(backboard), m_NextArtboardId(0)
{
}
void BackboardImporter::addNestedArtboard(NestedArtboard* artboard)
{
	m_NestedArtboards.push_back(artboard);
}

void BackboardImporter::addArtboard(Artboard* artboard)
{
	m_ArtboardLookup[m_NextArtboardId++] = artboard;
}
void BackboardImporter::addMissingArtboard() { m_NextArtboardId++; }

StatusCode BackboardImporter::resolve()
{
	for (auto nestedArtboard : m_NestedArtboards)
	{
		auto itr = m_ArtboardLookup.find(nestedArtboard->artboardId());
		if (itr != m_ArtboardLookup.end())
		{
			auto artboard = itr->second;
			/*
			if (artboard is RuntimeArtboard)
			{
			    (nestedArtboard as RuntimeNestedArtboard).sourceArtboard =
			artboard;
			}*/
		}
	}
}
