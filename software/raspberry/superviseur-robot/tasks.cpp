/*
 * Copyright (C) 2018 dimercur
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tasks.h"
#include <stdexcept>

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 30
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_CAMERAON 25
#define PRIORITY_CAMERAOFF 25
#define PRIORITY_TMOVE 20
#define PRIORITY_TSENDTOMON 22
#define PRIORITY_TRECEIVEFROMMON 25
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TCAMERA 21
#define PRIORITY_GESTIONVISION 21
#define PRIORITY_TCHECKBATTERY 23 
#define PRIORITY_WATCHDOG 50 

#define PRIORITY_RESETMON 60
#define PRIORITY_RESETWD 60





/*
 * Some remarks:
 * 1- This program is mostly a template. It shows you how to create tasks, semaphore
 *   message queues, mutex ... and how to use them
 * 
 * 2- semDumber is, as name say, useless. Its goal is only to show you how to use semaphore
 * 
 * 3- Data flow is probably not optimal
 * 
 * 4- Take into account that ComRobot::Write will block your task when serial buffer is full,
 *   time for internal buffer to flush
 * 
 * 5- Same behavior existe for ComMonitor::Write !
 * 
 * 6- When you want to write something in terminal, use cout and terminate with endl and flush
 * 
 * 7- Good luck !
 */

/**
 * @brief Initialisation des structures de l'application (tâches, mutex, 
 * semaphore, etc.)
 */
void Tasks::Init() {
    int status;
    int err;

    /**************************************************************************************/
    /* 	Mutex creation                                                                    */
    /**************************************************************************************/
    if (err = rt_mutex_create(&mutex_monitor, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robot, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robotStarted, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_move, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_mutex_create(&mutex_watchdog, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    if (err = rt_mutex_create(&mutex_lossdetector, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_mutex_create(&mutex_camera, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_mutex_create(&mutex_envoieperiodique, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    cout << "Mutexes created successfully" << endl << flush;

    /**************************************************************************************/
    /* 	Semaphors creation       							  */
    /**************************************************************************************/
    if (err = rt_sem_create(&sem_barrier, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_openComRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_serverOk, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_startRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_watchdog, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_sem_create(&sem_resetmon, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_sem_create(&sem_opencom, NULL, 1, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    if (err = rt_sem_create(&sem_resetwatchdog, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_sem_create(&sem_cameraOpen, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_sem_create(&sem_cameraClose, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    cout << "Semaphores created successfully" << endl << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if (err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_create(&th_checkBattery, "th_checkBattery", 0, PRIORITY_TCHECKBATTERY, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_create(&th_watchdog, "th_watchdog", 0, PRIORITY_WATCHDOG, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_create(&th_resetMon, "th_resetMon", 0, PRIORITY_RESETMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    if (err = rt_task_create(&th_resetWD, "th_resetWD", 0, PRIORITY_RESETWD, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_create(&th_cameraOn, "th_cameraOn", 0, PRIORITY_CAMERAON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_create(&th_cameraOff, "th_cameraOff", 0, PRIORITY_CAMERAOFF, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_create(&th_gestionvision, "th_gestionvision", 0, PRIORITY_GESTIONVISION, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    
    
    cout << "Tasks created successfully" << endl << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof (Message*)*50, Q_UNLIMITED, Q_FIFO)) < 0) {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl << flush;

}

/**
 * @brief Démarrage des tâches
 */
void Tasks::Run() {
    rt_task_set_priority(NULL, T_LOPRIO);
    int err;

    if (err = rt_task_start(&th_server, (void(*)(void*)) & Tasks::ServerTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_sendToMon, (void(*)(void*)) & Tasks::SendToMonTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_receiveFromMon, (void(*)(void*)) & Tasks::ReceiveFromMonTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_openComRobot, (void(*)(void*)) & Tasks::OpenComRobot, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_startRobot, (void(*)(void*)) & Tasks::StartRobotTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_move, (void(*)(void*)) & Tasks::MoveTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_start(&th_checkBattery, (void(*)(void*)) & Tasks::CheckBattery, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_start(&th_watchdog, (void(*)(void*)) & Tasks::Watchdog, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_start(&th_resetMon, (void(*)(void*)) & Tasks::ResetMon, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    if (err = rt_task_start(&th_resetWD, (void(*)(void*)) & Tasks::ResetWDThread, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_start(&th_cameraOn, (void(*)(void*)) & Tasks::Camera_On, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_start(&th_cameraOff, (void(*)(void*)) & Tasks::Camera_Off, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    if (err = rt_task_start(&th_gestionvision, (void(*)(void*)) & Tasks::GestionVision, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }


    cout << "Tasks launched" << endl << flush;
}

/**
 * @brief Arrêt des tâches
 */
void Tasks::Stop() {
    monitor.Close();
    robot.Close();
    camera.Close();
}

/**
 */
void Tasks::Join() {
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg) {
    int status;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are started)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task server starts here                                                        */
    /**************************************************************************************/
    while(1){
        rt_sem_p(&sem_opencom ,TM_INFINITE);
        
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        status = monitor.Open(SERVER_PORT);
        rt_mutex_release(&mutex_monitor);

        cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

        if (status < 0) throw std::runtime_error {
            "Unable to start server on port " + std::to_string(SERVER_PORT)
        };
        monitor.AcceptClient(); // Wait the monitor client
        cout << "Rock'n'Roll baby, client accepted!" << endl << flush;
        rt_sem_broadcast(&sem_serverOk);
    }
}

/**
 * @brief Thread sending data to monitor.
 */
void Tasks::SendToMonTask(void* arg) {
    Message *msg;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (1) {
        cout << "wait msg to send" << endl << flush;
        msg = ReadInQueue(&q_messageToMon);
        cout << "Send msg to mon: " << msg->ToString() << endl << flush;
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Write(msg); // The message is deleted with the Write
        rt_mutex_release(&mutex_monitor);
    }
}

/**
 * @brief Thread receiving data from monitor.
 */
void Tasks::ReceiveFromMonTask(void *arg) {
    Message *msgRcv;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    while (1) {
        
        rt_sem_p(&sem_serverOk, TM_INFINITE);
        cout << "Received message from monitor activated" << endl << flush;

        while (1) {
            msgRcv = monitor.Read();
            cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

            if (msgRcv->CompareID(MESSAGE_MONITOR_LOST)) {
                //delete(msgRcv);
                cout << "PERTE DE COMMUNICATION AVEC LE MONITEUR" << endl;
                rt_sem_v(&sem_resetmon);
                break;
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN)) {
                rt_sem_v(&sem_openComRobot);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD )){
                rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                watchdogActivated = 0;
                rt_mutex_release(&mutex_watchdog);
                rt_sem_v(&sem_startRobot);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD )){
                rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                watchdogActivated = 1;
                rt_mutex_release(&mutex_watchdog);
                rt_sem_v(&sem_startRobot);
                rt_task_set_periodic(&th_watchdog, TM_NOW, 1000000000);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                    msgRcv->CompareID(MESSAGE_ROBOT_STOP)) {

                rt_mutex_acquire(&mutex_move, TM_INFINITE);
                move = msgRcv->GetID();
                rt_mutex_release(&mutex_move);
            } else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_CLOSE)){
                //close comm with robot
                cout << "FERMETURE COMM ROBOT" << endl;
                rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
                if (watchdogActivated == 1){
                    rt_sem_v(&sem_resetwatchdog);
                    watchdogActivated = 0;
                }
                rt_mutex_release(&mutex_watchdog);


                rt_mutex_acquire(&mutex_robot, TM_INFINITE);
                //stopper robot

                robot.Write(robot.Stop());

                //reset robot
                robot.Write(robot.Reset());
                
                //fermer comm robot
                int status = robot.Close();
                rt_mutex_release(&mutex_robot);
                
                rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                robotStarted = 0;
                rt_mutex_release(&mutex_robotStarted);
                
                Message * msgSend;
                if (status < 0) {
                    msgSend = new Message(MESSAGE_ANSWER_NACK);
                } else {
                    msgSend = new Message(MESSAGE_ANSWER_ACK);
                }
                WriteInQueue(&q_messageToMon, msgSend);
                
            } else if (msgRcv->CompareID(MESSAGE_CAM_OPEN)){
                rt_sem_v(&sem_cameraOpen);
            } else if (msgRcv->CompareID(MESSAGE_CAM_CLOSE)){
                rt_sem_v(&sem_cameraClose);
            } 
            delete(msgRcv); // mus be deleted manually, no consumer
        }
    }
    
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg) {
    int status;
    int err;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (1) {
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
        cout << "Open serial com (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        status = robot.Open();
        rt_mutex_release(&mutex_robot);
        cout << status;
        cout << ")" << endl << flush;

        Message * msgSend;
        if (status < 0) {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
        } else {
            msgSend = new Message(MESSAGE_ANSWER_ACK);
        }
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
    }
}

/**
 * @brief Thread starting the communication with the robot.
 */
void Tasks::StartRobotTask(void *arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (1) {
        Message * msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);
        cout << "Start robot without watchdog (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
        if (watchdogActivated == 1){
            msgSend = robot.Write(robot.StartWithWD());
            rt_sem_v(&sem_watchdog);
        } else {
            msgSend = robot.Write(robot.StartWithoutWD());
        }
        rt_mutex_release(&mutex_watchdog);
        rt_mutex_release(&mutex_robot);
        cout << msgSend->GetID();
        cout << ")" << endl;

        cout << "Movement answer: " << msgSend->ToString() << endl << flush;
        WriteInQueue(&q_messageToMon, msgSend);  // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
        }
    }
}

/**
 * @brief Thread handling control of the robot.
 */
void Tasks::MoveTask(void *arg) {
    int rs;
    int cpMove;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    
    Message * msg;
    while (1) {
        rt_task_wait_period(NULL);
        cout << "Periodic movement update" <<endl;
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);
            
            cout << " move: " << cpMove;
            
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msg = robot.Write(new Message((MessageID)cpMove));
            LossDetector(msg);
            rt_mutex_release(&mutex_robot);
        }
        cout << endl << flush;
    }
}

/**
 * Write a message in a given queue
 * @param queue Queue identifier
 * @param msg Message to be stored
 */
void Tasks::WriteInQueue(RT_QUEUE *queue, Message *msg) {
    int err;
    if ((err = rt_queue_write(queue, (const void *) &msg, sizeof ((const void *) &msg), Q_NORMAL)) < 0) {
        cerr << "Write in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in write in queue"};
    }
}

/**
 * Read a message from a given queue, block if empty
 * @param queue Queue identifier
 * @return Message read
 */
Message *Tasks::ReadInQueue(RT_QUEUE *queue) {
    int err;
    Message *msg;

    if ((err = rt_queue_read(queue, &msg, sizeof ((void*) &msg), TM_INFINITE)) < 0) {
        cout << "Read in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in read in queue"};
    }/** else {
        cout << "@msg :" << msg << endl << flush;
    } /**/

    return msg;
}

/**
 * Retrieves the battery level
 * @param none
 * @return none
 */
void Tasks::CheckBattery(){
    
    int rs;
    MessageBattery * batterylevel;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 500000000);
    
    while(1){
        rt_task_wait_period(NULL);
        cout << "batterie vivante" << endl;
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            batterylevel = (MessageBattery*)robot.Write(new Message(MESSAGE_ROBOT_BATTERY_GET));
            LossDetector(batterylevel);
            if (batterylevel->GetID() != MESSAGE_ROBOT_BATTERY_LEVEL){
                cout << "Problème de récupération du niveau de batterie." << endl;
            }
            else
            {
                WriteInQueue(&q_messageToMon, batterylevel);
            }
            rt_mutex_release(&mutex_robot);
        }
        
    } 
}

void Tasks::Watchdog(){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_watchdog, TM_INFINITE);
    Message *msgReload;
    Message *msg;
    rt_task_set_periodic(NULL, TM_NOW, 1000000000);
    while(1){
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        
        if (robotStarted == 1){
            LossDetector(msg);
            msgReload = robot.ReloadWD();
            msg = robot.Write(msgReload);
        }
        cout << "MESSAGE RELOAD WATCHDOG ENVOYE" << endl;
        rt_mutex_release(&mutex_robot);
    }
}


void Tasks::ResetWDThread(){
    int err;
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    while(1) {
        rt_sem_p(&sem_resetwatchdog, TM_INFINITE);

//        if (err = rt_task_delete(&th_watchdog)) {
//            cerr << "Error task delete: " << strerror(-err) << endl << flush;
//            exit(EXIT_FAILURE);
//        }
//
//        if (err = rt_task_create(&th_watchdog, "th_watchdog", 0, PRIORITY_WATCHDOG, 0)) {
//            cerr << "Error task create: " << strerror(-err) << endl << flush;
//            exit(EXIT_FAILURE);
//        }
//
//        if (err = rt_task_start(&th_watchdog, (void (*)(void *)) &Tasks::Watchdog, this)) {
//            cerr << "Error task start: " << strerror(-err) << endl << flush;
//            exit(EXIT_FAILURE);
//        }
        rt_task_set_periodic(&th_watchdog, TM_NOW, 0);
        cout << "Task WD relance avec succes" << endl ;
    }
}
/**
* Detecs a loss of communication between the robot and the supervisor
* @param Message *message
* @return none
*/

// Fonction à appeler dès que l'on envoie un message au robot
void Tasks::LossDetector(Message *message){
    rt_mutex_acquire(&mutex_lossdetector, TM_INFINITE);

    // On vérifie s'il y a une erreur sur le message :
    if (message->GetID() == MESSAGE_ANSWER_ROBOT_ERROR | 
            message->GetID() == MESSAGE_ANSWER_COM_ERROR | 
            message->GetID() == MESSAGE_ANSWER_ROBOT_TIMEOUT | 
            message->GetID() == MESSAGE_ANSWER_ROBOT_UNKNOWN_COMMAND){
        losscounter ++;
        // On vérifie qu'il n'y a pas eu plus de 3 erreurs successives
        if (losscounter >=3 ){
            cout << "perte comm avec robot" << endl;
            ResetRobot();
            losscounter = 0;
        }
    }
    else{ // Tout s'est bien passé
        losscounter = 0;
    }
    rt_mutex_release(&mutex_lossdetector);

}
  
void Tasks::ResetMon(){
    while(1) {
        cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
        rt_sem_p(&sem_resetmon, TM_INFINITE);
        cout << "entrée fct" << endl;
        Message *RobotStopped;
        int closed;
        
        //fermer camera
        cout << "Fermeture camera" << endl;
        rt_sem_v(&sem_cameraClose);

        rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
        if (watchdogActivated == 1){
            rt_sem_v(&sem_resetwatchdog);
            watchdogActivated = 0;
        }
        rt_mutex_release(&mutex_watchdog);


        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        //stopper robot
        cout << "entrée mutex" << endl;
        RobotStopped = robot.Stop();
        robot.Write(RobotStopped);
        cout << "robot stop" << endl;

        //reset robot
        RobotStopped = robot.Reset();
        robot.Write(RobotStopped);
        cout << "robot reset" << endl;

        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        robotStarted = 0;
        cout << "Robot stoppé" << endl;
        rt_mutex_release(&mutex_robotStarted);

        //fermer comm robot
        closed = robot.Close();
        if (closed >= 0) {
            cout << "Communication avec robot stoppée" << endl;
        } else {
            cout << "Erreur fermeture comm avec robot" << endl;
            exit(-1);
        }

        rt_mutex_release(&mutex_robot);

        //remet losscounter a 0
        rt_mutex_acquire(&mutex_lossdetector, TM_INFINITE);
        losscounter = 0;
        rt_mutex_release(&mutex_lossdetector);

        
        
        //fermer serveur
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Close();
        rt_mutex_release(&mutex_monitor);
        rt_sem_v(&sem_opencom);
        
        
        
    }
} 

/**
* Closes the communication between the robot and the supervisor (initialize the variables)
* @param None
* @return none
*/
void Tasks::ResetRobot(){
    // On envoie un message au moniteur pouor prévenir de la perte de communication avec robot
    Message * LossCommunication = new Message(MESSAGE_ANSWER_COM_ERROR);
    WriteInQueue(&q_messageToMon, LossCommunication);
    cout << "MESSAGE ENVOYÉ AU MONITEUR" << endl;
    
    rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
    if (watchdogActivated == 1){
        rt_sem_v(&sem_resetwatchdog);
        watchdogActivated = 0;
    }
    rt_mutex_release(&mutex_watchdog);
    
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    robotStarted = 0;
    cout << "Robot stoppé" << endl;
    rt_mutex_release(&mutex_robotStarted);

}

void Tasks::Camera_On(){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    Message * msgSend;
    
    while(1){
        
        rt_sem_p(&sem_cameraOpen, TM_INFINITE);
        rt_mutex_acquire(&mutex_camera, TM_INFINITE);
        bool cameraOpen = camera.Open();
        rt_mutex_release(&mutex_camera);
        
        if (cameraOpen == true){
            msgSend = new Message(MESSAGE_ANSWER_ACK);
            WriteInQueue(&q_messageToMon, msgSend);
            
            rt_mutex_acquire(&mutex_envoieperiodique, TM_INFINITE);
            envoiePeriodique = 1;
            rt_mutex_release(&mutex_envoieperiodique);

        } else {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
            WriteInQueue(&q_messageToMon, msgSend);
            cout << "Camera not open" << endl;
        }

    }
}

void Tasks::Camera_Off(){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    while(1){
        //attente camera close
        rt_sem_p(&sem_cameraClose, TM_INFINITE);

        cout << "fermeture camera " << endl;
        rt_mutex_acquire(&mutex_envoieperiodique, TM_INFINITE);
        envoiePeriodique = 0;
        rt_mutex_release(&mutex_envoieperiodique);

        rt_mutex_acquire(&mutex_camera, TM_INFINITE);
        camera.Close();
        rt_mutex_release(&mutex_camera);
            

    }
}

void Tasks::GestionVision(){
    
    int envoiPer;
    
    MessageImg * messImg;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    
    while(1){
        rt_task_wait_period(NULL);
        
        rt_mutex_acquire(&mutex_envoieperiodique, TM_INFINITE);
        envoiPer = envoiePeriodique;
        rt_mutex_release(&mutex_envoieperiodique);
        if (envoiPer == 1) {
            rt_mutex_acquire(&mutex_camera, TM_INFINITE);
            Img img = camera.Grab();
            rt_mutex_release(&mutex_camera);
            
            messImg = new MessageImg(MESSAGE_CAM_IMAGE, &img);
            
            monitor.Write(messImg);
            
        }
        
    } 
}
