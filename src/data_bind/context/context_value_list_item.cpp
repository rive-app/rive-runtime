#include "rive/data_bind/context/context_value_list_item.hpp"

using namespace rive;

DataBindContextValueListItem::DataBindContextValueListItem(
    std::unique_ptr<ArtboardInstance> artboard,
    std::unique_ptr<StateMachineInstance> stateMachine,
    ViewModelInstanceListItem* listItem) :
    m_Artboard(std::move(artboard)),
    m_StateMachine(std::move(stateMachine)),
    m_ListItem(listItem){};