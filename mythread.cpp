#include "mythread.h"
//#include "databaseop.h"
//#include <QMutexLocker>
#include "mycrawl.h"
#include "common.h"

#include <QMutex>
QMutex g_urlmutex;
QMutex g_crawlmutex;

#ifndef MYSQL
QMutex g_mutex;
#endif
int MyThread::thread_flag = THREAD_STOP; //

MyThread::MyThread(QObject *parent) : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
}

MyThread::~MyThread()
{
    delete m_manager;
}

/* 爬虫任务线程工作函数 */
void MyThread::slot_StartMyThread(QString rootURL,uint nWebType)
{
    //打印线程ID
    //qDebug() << "kid  thread : " << QThread::currentThread();
    qDebug() <<"["<<QThread::currentThread()<<"]thread start work," <<"root url:"<<rootURL;

    QString visited_table;
    QString todo_table;
    switch (nWebType)
    {
    case WEBTYPE_YINHUA:
        visited_table = TABLE_VISITED_YINHUA;
        todo_table = TABLE_TODO_YINHUA;
        break;
    case WEBTYPE_DIANYINTT:
        visited_table = TABLE_VISITED_DIANYINTT;
        todo_table = TABLE_TODO_DIANYINTT;
        break;
    default:
        return;
    }

    // 爬取根URL，获取任务列表
#ifdef MYSQL
    //Mycrawl mycrawl(rootURL);
    //mycrawl.get(m_manager, nWebType);
#else
    Mycrawl mycrawl(rootURL);
    mycrawl.get(m_manager, nWebType);
#endif
    // 开始任务
    while (1)
    {
        if(thread_flag==THREAD_STOP)
        {
            qDebug()<<"["<<QThread::currentThread()<<"]thread exit";
            return;
        }
        if(thread_flag==THREAD_WAIT)
        {
            //qDebug()<<"["<<QThread::currentThread()<<"]thread wait";
            QThread::sleep(1);
            continue;
        }
        if(thread_flag == THREAD_CONTINUE)
        {
            qDebug()<<"All Thread Continue......";
            thread_flag = THREAD_START;
        }

        QString todo_url;
        bool isRepeat = false;
        /***************** URL去重 *****************/
        do
        {
#ifdef MYSQL
            //todo_url = DatabaseOp::Todo_PoP(TABLE_TODO_YINHUA);
            g_urlmutex.lock();
            todo_url = Pop_Todo(todo_table);                // 从队列取任务
            g_urlmutex.unlock();
            if (todo_url.isNull())
            {
                qDebug()<<"任务完成,等待添加任务......";
                continue;   // 多线程
                //qDebug() << "work over";
                //return; // 任务完成
            }

            //if (DatabaseOp::isExist(TABLE_VISITED_YINHUA, todo_url))
            if (IsExist(visited_table, todo_url))    // ！=0 访问表中存在
            {
                qDebug()<<"Visited URL, continue";
                continue;
            }
#else
            if(MyTable::GetInstance()->TodoTableIsEmpty())
            {
                //qDebug()<<"work over";
                return; // 任务完成
            }
            g_mutex.lock();
            todo_url = MyTable::GetInstance()->PopTodoTable();
            g_mutex.unlock();
            // URL 去重
            for(auto i : MyTable::GetInstance()->GetVisitedTable())
            {
                // 如果重复，跳出取下一个
                if(todo_url.compare(i)==0)
                {
                    isRepeat = true;
                    break;
                }
            }
            if(isRepeat)
            {
                isRepeat = false;
                continue;
            }
#endif
        }while(isRepeat);// 爬取不重复url

#ifdef MYSQL
        //DatabaseOp::Visited_Push(TABLE_VISITED_YINHUA, todo_url);
        //Push_Visited(visited_table, todo_url);// 加入已访问列表  改在爬取页面后加入
        g_crawlmutex.lock();
        Mycrawl todo_crawl(todo_url);
        g_crawlmutex.unlock();
        todo_crawl.get(m_manager, nWebType);
#else
        {
            QMutexLocker mutexlocker(&g_mutex);
            MyTable::GetInstance()->PushVisitedTable(todo_url);

            //URL未使用，爬虫工作
            Mycrawl todo_crawl(todo_url);
            todo_crawl.get(m_manager, nWebType);
        }
#endif
        // 休息10S
        //QThread::sleep(10);
    }

    //返回线程完成信号
    //emit Threadfinish();

}

void MyThread::on_thread_finished_btn_clicked()
{
    thread_flag = THREAD_STOP;
}

void MyThread::on_thread_wait_btn_clicked()
{
    qDebug()<<"All Thread Waiting......";
    thread_flag = THREAD_WAIT;
}

void MyThread::on_thread_continue_btn_clicked()
{
    thread_flag = THREAD_CONTINUE;
}
