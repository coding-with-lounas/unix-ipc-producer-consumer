# 🏥 Organ Transplant Simulation System

A multi-process synchronization simulation written in C that models an organ transplant coordination system using Unix/Linux IPC (Inter-Process Communication) mechanisms.

## 📋 Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [French to English Glossary](#french-to-english-glossary)
- [IPC Mechanisms](#ipc-mechanisms)
- [Components](#components)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [How It Works](#how-it-works)
- [Educational Purpose](#educational-purpose)

## Overview

This project simulates a hospital organ transplant coordination system with 4 concurrent processes:
- **Critical Patients** requesting urgent organ transplants
- **Non-Critical Patients** requesting regular organ transplants  
- **Surgeon** coordinating requests and performing implantations
- **Donor** processing orders and providing matching organs

## System Architecture

```
┌─────────────┐     Qcr (Message Queue)      ┌─────────────┐
│  MaladeCr   │─────────────────────────────▶│             │
│  (Critical  │                              │             │
│  Patients)  │◀─────────────────────────────│             │
└─────────────┘     Qimp (mtype=1)           │             │
                                             │ Chirurgien  │
┌─────────────┐     Qncr (Message Queue)     │  (Surgeon)  │
│ MaladeNCr   │─────────────────────────────▶│             │
│(Non-Critical│                              │             │
│  Patients)  │◀─────────────────────────────│             │
└─────────────┘     Qimp (mtype=2)           └──────┬──────┘
                                                    │
                                               Pipe │ (tube)
                                                    ▼
                                             ┌─────────────┐
                    Shared Memory Buffer     │   Donneur   │
                    ◀───────────────────────▶│   (Donor)   │
                    (Torgane[N])             └─────────────┘
```

## French to English Glossary

### Process Names

| French | English | Description |
|--------|---------|-------------|
| `MaladeCr` | Critical Patient | Patient requiring urgent transplant |
| `MaladeNCr` | Non-Critical Patient | Patient with regular priority |
| `Chirurgien` | Surgeon | Coordinates transplants |
| `Donneur` | Donor | Provides organs |

### Functions

| French | English | Description |
|--------|---------|-------------|
| `Creer_et_initialiser_semaphores()` | Create and initialize semaphores | Setup semaphores |
| `Detruire_semaphores()` | Destroy semaphores | Cleanup semaphores |
| `Creer_files_messages()` | Create message queues | Setup message queues |
| `Detruire_files_messages()` | Destroy message queues | Cleanup message queues |
| `Creer_et_attacher_tampon()` | Create and attach buffer | Setup shared memory buffer |
| `Detruire_tampon()` | Destroy buffer | Cleanup shared memory |
| `Creer_tube()` | Create pipe | Setup pipe |
| `Detruire_tube()` | Destroy pipe | Cleanup pipe |
| `Deposer()` | Deposit | Add organ to buffer |
| `Prelever()` | Retrieve/Remove | Take organ from buffer |
| `Agenerer()` | Generate (random) | Generate random boolean |

### Variables

| French | English | Description |
|--------|---------|-------------|
| `Torgane` | Organ buffer | Shared memory circular buffer |
| `Organe` | Organ | Struct for organ data |
| `Commande` | Order/Command | Struct for patient request |
| `tampon` | buffer | Circular buffer |
| `tube` | pipe | Unix pipe |
| `malade` | patient/sick person | Patient identifier |
| `organe` | organ | Organ identifier |
| `valide` | valid | Validity flag |
| `cpt` | counter (compteur) | Organ counter |
| `nv` | empty slots (nombre vide) | Semaphore for empty spaces |
| `mutex` | mutex | Mutual exclusion semaphore |

### Message Queues

| French | English | Description |
|--------|---------|-------------|
| `Qcr` | Critical Queue | Queue for critical patient requests |
| `Qncr` | Non-Critical Queue | Queue for non-critical patient requests |
| `Qimp` | Implantation Queue | Queue for implant confirmations |

### Constants

| French | English | Description |
|--------|---------|-------------|
| `N` | Buffer size | Capacity of circular buffer |
| `M1` | Critical patient count | Number of critical patients |
| `M2` | Non-critical patient count | Number of non-critical patients |
| `N1` | Critical organ types | Types of critical organs |
| `N2` | Non-critical organ types | Types of non-critical organs |

### Output Messages

| French | English |
|--------|---------|
| `Processus démarré` | Process started |
| `Processus terminé` | Process finished |
| `Commande envoyée` | Command sent |
| `Commande transmise` | Command transmitted |
| `Commande enregistrée` | Command registered |
| `Organe reçu` | Organ received |
| `Organe implanté` | Organ implanted |
| `Organe livré` | Organ delivered |
| `Ressources IPC créées` | IPC resources created |
| `Ressources IPC libérées` | IPC resources freed |

## IPC Mechanisms

| Mechanism | Variable | Purpose |
|-----------|----------|---------|
| **Semaphores** | `mutex` | Mutual exclusion for buffer access |
| | `nv` | Counting semaphore (empty slots in buffer) |
| **Message Queues** | `Qcr` | Critical patient requests |
| | `Qncr` | Non-critical patient requests |
| | `Qimp` | Implantation confirmations |
| **Shared Memory** | `Torgane[N]` | Circular buffer for organs |
| | `cpt` | Counter of organs in buffer |
| | `head_pos` | Read position (consumer) |
| | `tail_pos` | Write position (producer) |
| **Pipe** | `tube[2]` | Surgeon → Donor communication |

## Components

| Process | Role | Description |
|---------|------|-------------|
| `MaladeCr()` | Critical Patients | Send urgent organ requests |
| `MaladeNCr()` | Non-Critical Patients | Send regular organ requests |
| `Chirurgien()` | Surgeon | Coordinates requests, transmits to donor, implants organs |
| `Donneur()` | Donor | Receives orders, generates matching organs, deposits in buffer |

## Installation

### Prerequisites

- Linux/Unix operating system
- GCC compiler
- Standard C libraries (stdio, stdlib, unistd, sys/ipc, sys/msg, sys/shm, sys/sem)

### Compilation

```bash
gcc -Wall -Wextra -o tp2 tp2.c
```

## Usage

```bash
./tp2
```

### Sample Output

```
=== Systeme de Transplantation d'Organes ===
Malades critiques: 5, Non-critiques: 6
Organes critiques: 3, Non-critiques: 4

Ressources IPC creees

[MaladeCr] Processus 10231 démarré
[MaladeNCr] Processus 10232 démarré
[Chirurgien] Processus 10233 démarré
[Donneur] Processus 10234 démarré
[MaladeCr] Commande envoyée: malade=1, organe=1
...
[Chirurgien] Organe implanté: type=1, malade=1, organe=1
...
[MaladeCr] Processus terminé (reçu 5 organes)
[MaladeNCr] Processus terminé (reçu 6 organes)
[Chirurgien] Processus terminé (11 organes implantés)
[Donneur] Processus terminé (11 organes livrés)

=== Fin du programme ===
Ressources IPC liberees
```

## Configuration

Edit the constants in `tp2.c` to customize the simulation:

```c
#define N   5   // Buffer size (circular buffer capacity)
#define M1  5   // Number of critical patients
#define M2  6   // Number of non-critical patients
#define N1  3   // Types of critical organs (keep small for faster execution)
#define N2  4   // Types of non-critical organs (keep small for faster execution)
```

> ⚠️ **Note:** Keep `N1` and `N2` values small (≤10) for efficient execution. Large values increase the time needed for random organ matching.

## How It Works

### Data Flow

1. **Patients** send organ requests via message queues (`Qcr`/`Qncr`)
2. **Surgeon** receives requests and forwards them to donor via **pipe**
3. **Donor** generates matching organs and deposits them in the **shared memory buffer**
4. **Surgeon** retrieves organs from buffer using semaphore synchronization
5. **Surgeon** sends implantation confirmation via `Qimp` message queue
6. **Patients** receive confirmation of successful transplant

### Synchronization

- **Producer-Consumer Pattern**: Donor produces organs, Surgeon consumes them
- **Circular Buffer**: Managed with `head_pos` (read) and `tail_pos` (write) pointers
- **Semaphores**: 
  - `mutex`: Ensures mutual exclusion when accessing shared buffer
  - `nv`: Counts available empty slots in buffer (initialized to N)

### Key Functions

| Function | Description |
|----------|-------------|
| `P(semid)` | Decrement semaphore (wait/acquire) |
| `V(semid)` | Increment semaphore (signal/release) |
| `Deposer(org)` | Add organ to circular buffer |
| `Prelever(org)` | Remove organ from circular buffer |

## Educational Purpose

This project demonstrates practical understanding of:

- ✅ Process creation and management (`fork()`, `wait()`)
- ✅ Inter-process communication (IPC)
- ✅ Semaphore synchronization (`semget`, `semop`, `semctl`)
- ✅ Message queues (`msgget`, `msgsnd`, `msgrcv`)
- ✅ Shared memory (`shmget`, `shmat`, `shmdt`)
- ✅ Pipes (`pipe`, `read`, `write`)
- ✅ Producer-Consumer problem
- ✅ Resource cleanup and IPC destruction

## 📁 Project Structure

```
.
├── tp2.c          # Main source code
├── tp2            # Compiled executable
└── README.md      # This file
```

## 🧹 Cleanup

The program automatically cleans up all IPC resources on exit:
- Semaphores
- Message queues  
- Shared memory segments
- Pipe file descriptors

If the program terminates abnormally, you can manually clean IPC resources:

```bash
# List IPC resources
ipcs

# Remove specific resources
ipcrm -s <semid>    # Remove semaphore
ipcrm -q <msgid>    # Remove message queue
ipcrm -m <shmid>    # Remove shared memory
```

## License

This project is for educational purposes as part of an Operating Systems (Systèmes d'Exploitation) course.

---

**TP Systèmes d'Exploitation**

**Language:** C  
**Platform:** Linux/Unix
