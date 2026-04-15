# Adaptive CPU Scheduler

## Aim
The aim of this project is to design and implement an advanced adaptive CPU scheduling framework that dynamically selects and adjusts scheduling strategies based on real-time workload characteristics.

---

## Project Description

This project implements a comprehensive CPU scheduling framework that integrates classical scheduling algorithms with an adaptive decision engine capable of selecting the most suitable scheduling strategy at runtime.

### Implemented Scheduling Algorithms
The system includes implementations of the following classical CPU scheduling algorithms:

- First Come First Serve (FCFS)
- Shortest Job First (Preemptive)
- Shortest Job First (Non-Preemptive)
- Round Robin
- Priority Scheduling

---

### Adaptive Decision Engine

An intelligent **Adaptive Decision Engine** dynamically selects the most appropriate scheduling algorithm based on real-time workload characteristics such as:

- Average burst time
- Burst time variance
- Waiting time thresholds
- System load
- Context switch frequency

This allows the scheduler to adapt to varying workload patterns and improve overall system efficiency.

---

### Starvation Prevention

To ensure fair CPU allocation among processes, the system integrates:

- Starvation detection mechanisms
- Configurable waiting time thresholds
- Priority boosting techniques

These mechanisms prevent processes from waiting indefinitely.

---

### Aging Mechanism

An **aging mechanism** is implemented in priority scheduling to gradually increase the priority of processes that have been waiting for long periods, preventing indefinite postponement.

---

### Dynamic Time Quantum Adjustment

A **Dynamic Time Quantum Adjustment Module** modifies the Round Robin time quantum based on workload statistics.  

This helps:

- Reduce excessive context switching
- Improve system responsiveness
- Maintain efficient CPU utilization

---

### System Performance Monitoring

The framework tracks CPU idle time and computes **CPU utilization** to evaluate system efficiency.

Performance metrics calculated include:

- Waiting Time
- Turnaround Time
- Response Time
- Throughput
- Context Switch Count

---

### Fairness Evaluation

Scheduling fairness is evaluated using **Jain’s Fairness Index**, which quantitatively measures how evenly CPU resources are distributed among processes.

---

## Technologies Used

- C++
- Python
- Flask
- Operating Systems Scheduling Concepts
- Makefile Build System

---

##  To Run

### Install Dependencies

```bash
sudo apt install g++ make python3 python3-pip -y
pip install flask --break-system-packages

cd smart_scheduler
make
python3 gui/app.py

