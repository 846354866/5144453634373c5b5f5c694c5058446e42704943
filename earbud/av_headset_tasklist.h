/*!
\copyright  Copyright (c) 2008 - 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_tasklist.h
\brief      Interface to simple list of VM tasks.
*/

#ifndef AV_HEADSET_TASKLIST_H
#define AV_HEADSET_TASKLIST_H

/*! \brief Types of TaskList.
 */
typedef enum
{
    /*! Standard list of Tasks. */
    TASKLIST_TYPE_STANDARD,

    /*! TaskList with associated data. */
    TASKLIST_TYPE_WITH_DATA
} TaskListType;

/*! 64-bit TaskListData type */
typedef unsigned long long tl_uint64;

/*! \brief Definition of 'data' that can be stored in a TaskListWithData.
 */
typedef union
{
    /*! 8-bit data. */
    uint8 u8;

    /*! 16-bit data. */
    uint16 u16;

    /*! 32-bit data. */
    uint32 u32;

    /*! Any 32-bit pointer data. */
    void* ptr;

    /*! 64-bit data. */
    tl_uint64 u64;
} TaskListData;

/*! \brief List of VM Tasks.
 */
typedef struct
{
    /*! List of tasks. */
    Task* tasks;

    /*! Number of tasks in #tasks. */
    uint16 size_list;

    /*! List of data items. */
    TaskListData* data;

    /*! Standard TaskList or one that can support data. */
    TaskListType list_type;
} TaskList;

/*! \brief Create a TaskList.

    \return TaskList* Pointer to new TaskList.
 */
TaskList* appTaskListInit(void);

/*! \brief Create a TaskList that can also store associated data.

    \return TaskList* Pointer to new TaskList.
 */
TaskList* appTaskListWithDataInit(void);

/*! \brief Destroy a TaskList.

    \param list [IN] Pointer to a Tasklist.

    \return bool TRUE TaskList destroyed successfully.
                 FALSE TaskList destroy failure.

 */
void appTaskListDestroy(TaskList* list);

/*! \brief Add a task to a list.
 
    \param list [IN] Pointer to a Tasklist.
    \param add_task [IN] Task to add to the list.

    \return FALSE if the task is already on the list, otherwise TRUE.
 */
bool appTaskListAddTask(TaskList* list, Task add_task);

/*! \brief Add a task and data to a list.
 
    \param list [IN] Pointer to a Tasklist.
    \param add_task [IN] Task to add to the list.
    \param data [IN] Data to store with the task on the list.

    \return FALSE if the task is already on the list, otherwise TRUE.
 */
bool appTaskListAddTaskWithData(TaskList* list, Task add_task, const TaskListData* data);

/*! \brief Remove a task from a list.

    \param list [IN] Pointer to a Tasklist.
    \param del_task [IN] Task to remove from the list.

    \return FALSE if the task was not on the list, otherwise TRUE.
 */
bool appTaskListRemoveTask(TaskList* list, Task del_task);

/*! \brief Return number of tasks in list.
 
    \param list [IN] Pointer to a Tasklist.

    \return uint16 Number of Tasks in the list. 
 */
uint16 appTaskListSize(TaskList* list);

/*! \brief Iterate through all tasks in a list.

    Pass NULL to next_task to start iterating at first task in the list.
    On each subsequent call next_task should be the task previously returned
    and appTaskListIterate will return the next task in the list.

    \param list [IN] Pointer to a Tasklist.
    \param next_task [IN/OUT] Pointer to task from which to iterate.

    \return bool TRUE next_task is returning a Task.
                 FALSE end of list, next_task is not returning a Task.
 */
bool appTaskListIterate(TaskList* list, Task* next_task);

/*! \brief Iterate through all tasks in a list returning data as well.
    
    Pass NULL to next_task to start iterating at first task in the list.
    On each subsequent call next_task should be the task previously returned
    and appTaskListIterate will return the next task in the list.

    \param list [IN] Pointer to a Tasklist.
    \param next_task [IN/OUT] Pointer to task from which to iterate.
    \param data [OUT] Pointer in which data associated with the task is returned.

    \return bool TRUE next_task is returning a Task.
                 FALSE end of list, next_task is not returning a Task.
 */
bool appTaskListIterateWithData(TaskList* list, Task* next_task, TaskListData* data);

/*! \brief Determine if a task is on a list.

    \param list [IN] Pointer to a Tasklist.
    \param search_task [IN] Task to search for on list.

    \return bool TRUE search_task is on list, FALSE search_task is not on the list.
 */
bool appTaskListIsTaskOnList(TaskList* list, Task search_task);

/*! \brief Create a duplicate task list.

    \param list [IN] Pointer to a Tasklist.

    \return TaskList * Pointer to duplicate task list.
 */
TaskList *appTaskListDuplicate(TaskList* list);

/*! \brief Send a message (with message body) to all tasks in the task list.

    \param list [IN] Pointer to a TaskList.
    \param id The message ID to send to the TaskList.
    \param data Pointer to the message content.
    \param size_data The sizeof the message content.
*/
void appTaskListMessageSendWithSize(TaskList *list, MessageId id, void *data, uint16 size_data);

/*! \brief Send a message (with data) to all tasks in the task list.
 
    \param list [IN] Pointer to a TaskList.
    \param id The message ID to send to the TaskList.
    \param message Pointer to the message content.

    \note Assumes id is of a form such that appending a _T to the id creates the
    message structure type string.
*/
#define appTaskListMessageSend(list, id, message) \
    appTaskListMessageSendWithSize(list, id, message, sizeof(id##_T))

/*! \brief Send a message without content to all tasks in the task list.

    \param list [IN] Pointer to a TaskList.
    \param id The message ID to send to the TaskList.
*/
#define appTaskListMessageSendId(list, id) \
    appTaskListMessageSendWithSize(list, id, NULL, 0)

/*! \brief Get the data stored in the list for a given task.

    \param list [IN] Pointer to a Tasklist.
    \param search_task [IN] Task to search for on list.
    \param data [OUT] Pointer to return the associated task data.

    \return bool TRUE search_task is on the list and data returned.
                 FALSE search_task is not on the list.
*/
bool appTaskListGetDataForTask(TaskList* list, Task search_task, TaskListData* data);

/*! \brief Set the data stored in the list for a given task.

    \param list [IN] Pointer to a Tasklist.
    \param search_task [IN] Task to search for on list.
    \param data [IN] Pointer to new associated task data.

    \return bool TRUE search_task is on the list and data changed.
                 FALSE search_task is not on the list.
*/
bool appTaskListSetDataForTask(TaskList* list, Task update_data, const TaskListData* data);

/*! \brief Determine if the list is one that supports data.

    \param list [IN] Pointer to a Tasklist.

    \return bool TRUE list supports data, FALSE list does not support data.
*/
bool appTaskListIsTaskListWithData(TaskList* list);

#endif /* AV_HEADSET_TASKLIST_H */


