#include <algorithm>
#include <iostream>
#include <string>
#include <limits>
#include <iomanip>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

// -------------------- ENUMS --------------------
enum Category { EMERGENCY = 1, URGENT = 2, ROUTINE = 3 };
enum ServiceType { CONSULTATION = 1, BED_ALLOCATION = 2, XRAY = 3 };

string categoryName(Category c) {
    switch (c) {
        case EMERGENCY: return "Emergency";
        case URGENT: return "Urgent";
        case ROUTINE: return "Routine";
        default: return "Unknown";
    }
}

string serviceName(ServiceType s) {
    switch (s) {
        case CONSULTATION: return "Consultation";
        case BED_ALLOCATION: return "Bed Allocation";
        case XRAY: return "X-Ray";
        default: return "Unknown";
    }
}

// -------------------- REQUEST LINKED LIST --------------------
struct Request {
    string id, patient, service, category, status, assignedResource;
    int resourceIndex;
    Request* next;

    Request(string requestId, string patientName, string serviceNameValue,
            string categoryNameValue)
        : id(requestId), patient(patientName), service(serviceNameValue),
          category(categoryNameValue), status("Pending"),
          assignedResource("Not Assigned"), resourceIndex(-1), next(nullptr) {}
};

class RequestList {
private:
    Request* head;
public:
    RequestList() : head(nullptr) {}
    ~RequestList() {
        while (head != nullptr) {
            Request* temp = head;
            head = head->next;
            delete temp;
        }
    }

    void addRequest(const string& id, const string& patient,
                    const string& service, const string& category) {
        Request* node = new Request(id, patient, service, category);
        if (head == nullptr) {
            head = node;
            return;
        }
        Request* temp = head;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = node;
    }

    Request* searchRequest(const string& id) const {
        Request* temp = head;
        while (temp != nullptr) {
            if (temp->id == id) return temp;
            temp = temp->next;
        }
        return nullptr;
    }

    bool updatePatient(const string& id, const string& newName) {
        Request* request = searchRequest(id);
        if (request == nullptr || request->status != "Pending") return false;
        request->patient = newName;
        return true;
    }

    // Called only after the request has been removed from its priority queue.
    bool removeRequest(const string& id) {
        Request* current = head;
        Request* previous = nullptr;
        while (current != nullptr) {
            if (current->id == id) {
                if (previous == nullptr) head = current->next;
                else previous->next = current->next;
                delete current;
                return true;
            }
            previous = current;
            current = current->next;
        }
        return false;
    }

    void displayRequests() const {
        if (head == nullptr) {
            cout << "No service requests found.\n";
            return;
        }
        cout << "\n" << left << setw(10) << "ID" << setw(18) << "Patient"
             << setw(18) << "Service" << setw(13) << "Category"
             << setw(16) << "Status" << "Assigned Resource\n";
        cout << string(92, '-') << "\n";
        Request* temp = head;
        while (temp != nullptr) {
            cout << left << setw(10) << temp->id << setw(18) << temp->patient
                 << setw(18) << temp->service << setw(13) << temp->category
                 << setw(16) << temp->status << temp->assignedResource << "\n";
            temp = temp->next;
        }
    }

    int countStatus(const string& wantedStatus) const {
        int count = 0;
        Request* temp = head;
        while (temp != nullptr) {
            if (temp->status == wantedStatus) ++count;
            temp = temp->next;
        }
        return count;
    }

    int countPendingService(const string& wantedService) const {
        int count = 0;
        Request* temp = head;
        while (temp != nullptr) {
            if (temp->status == "Pending" && temp->service == wantedService) ++count;
            temp = temp->next;
        }
        return count;
    }
};

// -------------------- LINKED QUEUE --------------------
struct QueueNode {
    string requestID;
    QueueNode* next;
    explicit QueueNode(const string& id) : requestID(id), next(nullptr) {}
};

class RequestQueue {
private:
    QueueNode* frontNode;
    QueueNode* rearNode;
    int count;
public:
    RequestQueue() : frontNode(nullptr), rearNode(nullptr), count(0) {}
    ~RequestQueue() {
        string ignored;
        while (dequeue(ignored)) {}
    }

    bool isEmpty() const { return frontNode == nullptr; }
    int size() const { return count; }

    void enqueue(const string& id) {
        QueueNode* node = new QueueNode(id);
        if (rearNode == nullptr) frontNode = rearNode = node;
        else {
            rearNode->next = node;
            rearNode = node;
        }
        ++count;
    }

    bool dequeue(string& id) {
        if (isEmpty()) return false;
        QueueNode* temp = frontNode;
        id = temp->requestID;
        frontNode = frontNode->next;
        if (frontNode == nullptr) rearNode = nullptr;
        delete temp;
        --count;
        return true;
    }

    // Removes a particular request without disturbing the order of other IDs.
    bool removeById(const string& id) {
        bool removed = false;
        int originalSize = count;
        for (int i = 0; i < originalSize; ++i) {
            string currentID;
            dequeue(currentID);
            if (!removed && currentID == id) removed = true;
            else enqueue(currentID);
        }
        return removed;
    }

    void display(const string& title) const {
        cout << title << ": ";
        if (isEmpty()) {
            cout << "Empty\n";
            return;
        }
        QueueNode* temp = frontNode;
        while (temp != nullptr) {
            cout << temp->requestID;
            if (temp->next != nullptr) cout << " -> ";
            temp = temp->next;
        }
        cout << "\n";
    }
};

// -------------------- RESOURCE ARRAYS --------------------
struct Resource {
    string name;
    bool available;
    string requestID;
    Resource() : name(""), available(true), requestID("") {}
};

class ResourceManager {
public:
    static const int DOCTOR_COUNT = 5;
    static const int BED_COUNT = 10;
    static const int XRAY_COUNT = 2;

private:
    Resource doctors[DOCTOR_COUNT];
    Resource beds[BED_COUNT];
    Resource xrayMachines[XRAY_COUNT];

    void initialize(Resource arr[], int size, const string& prefix) {
        for (int i = 0; i < size; ++i) {
            arr[i].name = prefix + " " + to_string(i + 1);
            arr[i].available = true;
            arr[i].requestID = "";
        }
    }

public:
    ResourceManager() {
        initialize(doctors, DOCTOR_COUNT, "Doctor");
        initialize(beds, BED_COUNT, "Bed");
        initialize(xrayMachines, XRAY_COUNT, "X-Ray Machine");
    }

    bool allocate(ServiceType service, const string& requestID,
                  string& assignedName, int& assignedIndex) {
        Resource* arr = nullptr;
        int size = 0;
        if (service == CONSULTATION) { arr = doctors; size = DOCTOR_COUNT; }
        else if (service == BED_ALLOCATION) { arr = beds; size = BED_COUNT; }
        else if (service == XRAY) { arr = xrayMachines; size = XRAY_COUNT; }
        else return false;

        for (int i = 0; i < size; ++i) {
            if (arr[i].available) {
                arr[i].available = false;
                arr[i].requestID = requestID;
                assignedName = arr[i].name;
                assignedIndex = i;
                return true;
            }
        }
        return false;
    }

    bool release(ServiceType service, int index, const string& requestID) {
        Resource* arr = nullptr;
        int size = 0;
        if (service == CONSULTATION) { arr = doctors; size = DOCTOR_COUNT; }
        else if (service == BED_ALLOCATION) { arr = beds; size = BED_COUNT; }
        else if (service == XRAY) { arr = xrayMachines; size = XRAY_COUNT; }
        else return false;

        if (index < 0 || index >= size || arr[index].available ||
            arr[index].requestID != requestID) return false;
        arr[index].available = true;
        arr[index].requestID.clear();
        return true;
    }

    int availableCount(ServiceType service) const {
        const Resource* arr = nullptr;
        int size = 0;
        if (service == CONSULTATION) { arr = doctors; size = DOCTOR_COUNT; }
        else if (service == BED_ALLOCATION) { arr = beds; size = BED_COUNT; }
        else if (service == XRAY) { arr = xrayMachines; size = XRAY_COUNT; }
        else return 0;
        int count = 0;
        for (int i = 0; i < size; ++i) if (arr[i].available) ++count;
        return count;
    }

    int totalCount(ServiceType service) const {
        if (service == CONSULTATION) return DOCTOR_COUNT;
        if (service == BED_ALLOCATION) return BED_COUNT;
        if (service == XRAY) return XRAY_COUNT;
        return 0;
    }

    void showOne(const Resource arr[], int size, const string& title) const {
        cout << "\n" << title << "\n";
        for (int i = 0; i < size; ++i) {
            cout << "  " << left << setw(18) << arr[i].name
                 << (arr[i].available ? "Available" : "Occupied by " + arr[i].requestID)
                 << "\n";
        }
    }

    void displayResources() const {
        showOne(doctors, DOCTOR_COUNT, "Doctors:");
        showOne(beds, BED_COUNT, "Beds:");
        showOne(xrayMachines, XRAY_COUNT, "X-Ray Machines:");
    }

    void displayUtilization() const {
        const ServiceType types[] = { CONSULTATION, BED_ALLOCATION, XRAY };
        for (ServiceType type : types) {
            int total = totalCount(type);
            int used = total - availableCount(type);
            double percent = total == 0 ? 0.0 : (100.0 * used / total);
            cout << serviceName(type) << ": " << used << "/" << total
                 << " occupied (" << fixed << setprecision(1) << percent << "%)\n";
        }
    }
};

// -------------------- COMPLETED REQUEST HISTORY STACK --------------------
struct HistoryNode {
    string id, patient, service, category, resource;
    HistoryNode* next;
    explicit HistoryNode(Request* request)
        : id(request->id), patient(request->patient), service(request->service),
          category(request->category), resource(request->assignedResource), next(nullptr) {}
};

class HistoryStack {
private:
    HistoryNode* topNode;
public:
    HistoryStack() : topNode(nullptr) {}
    ~HistoryStack() {
        while (topNode != nullptr) {
            HistoryNode* temp = topNode;
            topNode = topNode->next;
            delete temp;
        }
    }

    void push(Request* request) {
        HistoryNode* node = new HistoryNode(request);
        node->next = topNode;
        topNode = node;
    }

    bool pop() {
        if (topNode == nullptr) return false;
        HistoryNode* temp = topNode;
        cout << "Removed latest history entry: " << temp->id << " - " << temp->patient << "\n";
        topNode = topNode->next;
        delete temp;
        return true;
    }

    void display() const {
        if (topNode == nullptr) {
            cout << "History is empty.\n";
            return;
        }
        cout << "\nCompleted Requests (latest first):\n";
        HistoryNode* temp = topNode;
        while (temp != nullptr) {
            cout << temp->id << " | " << temp->patient << " | " << temp->service
                 << " | " << temp->category << " | " << temp->resource << "\n";
            temp = temp->next;
        }
    }
};

// -------------------- PRIORITY SCHEDULER --------------------
class Scheduler {
private:
    RequestQueue emergencyQueue, urgentQueue, routineQueue;
    RequestList& requests;
    ResourceManager& resources;
    HistoryStack& history;

    RequestQueue* queueFor(const string& category) {
        if (category == "Emergency") return &emergencyQueue;
        if (category == "Urgent") return &urgentQueue;
        if (category == "Routine") return &routineQueue;
        return nullptr;
    }

    bool processQueue(RequestQueue& queue) {
        // Try each request that was waiting at the start of this pass once.
        // An unavailable resource no longer blocks other service types in this queue.
        int requestsToCheck = queue.size();
        for (int i = 0; i < requestsToCheck; ++i) {
            string id;
            if (!queue.dequeue(id)) return false;
            Request* request = requests.searchRequest(id);
            if (request == nullptr || request->status != "Pending") continue;

            ServiceType service;
            if (request->service == "Consultation") service = CONSULTATION;
            else if (request->service == "Bed Allocation") service = BED_ALLOCATION;
            else if (request->service == "X-Ray") service = XRAY;
            else {
                queue.enqueue(id);
                continue;
            }

            string resourceName;
            int resourceIndex = -1;
            if (resources.allocate(service, id, resourceName, resourceIndex)) {
                request->status = "In Progress";
                request->assignedResource = resourceName;
                request->resourceIndex = resourceIndex;
                cout << "Request " << id << " allocated to " << resourceName << ".\n";
                return true;
            }
            queue.enqueue(id); // Keep waiting, but let the next request be checked.
        }
        return false;
    }

public:
    Scheduler(RequestList& list, ResourceManager& resourceManager, HistoryStack& stack)
        : requests(list), resources(resourceManager), history(stack) {}

    void addToQueue(Request* request) {
        RequestQueue* queue = queueFor(request->category);
        if (queue != nullptr) queue->enqueue(request->id);
    }

    void displayQueues() const {
        emergencyQueue.display("Emergency Queue");
        urgentQueue.display("Urgent Queue");
        routineQueue.display("Routine Queue");
    }

    bool removeFromQueue(const string& id, const string& category) {
        RequestQueue* queue = queueFor(category);
        return queue != nullptr && queue->removeById(id);
    }

    void processNextRequest() {
        if (processQueue(emergencyQueue)) return;
        if (processQueue(urgentQueue)) return;
        if (processQueue(routineQueue)) return;
        cout << "No pending request can be processed with the currently available resources.\n";
    }

    bool completeRequest(const string& id) {
        Request* request = requests.searchRequest(id);
        if (request == nullptr) {
            cout << "Request ID not found.\n";
            return false;
        }
        if (request->status != "In Progress") {
            cout << "Only an In Progress request can be completed.\n";
            return false;
        }

        ServiceType service;
        if (request->service == "Consultation") service = CONSULTATION;
        else if (request->service == "Bed Allocation") service = BED_ALLOCATION;
        else if (request->service == "X-Ray") service = XRAY;
        else return false;

        if (!resources.release(service, request->resourceIndex, id)) {
            cout << "Resource release failed. Check the assigned resource record.\n";
            return false;
        }
        request->status = "Completed";
        history.push(request);
        cout << "Request " << id << " completed. Resource released and history updated.\n";
        return true;
    }
};

// -------------------- SIMULATION / STATISTICS --------------------
class HospitalSimulation {
private:
    RequestList& requests;
    ResourceManager& resources;
public:
    HospitalSimulation(RequestList& list, ResourceManager& manager)
        : requests(list), resources(manager) {}

    void showStatistics() const {
        int total = requests.countStatus("Pending") + requests.countStatus("In Progress")
                  + requests.countStatus("Completed");
        cout << "\n========== SYSTEM STATISTICS ==========\n";
        cout << "Total requests : " << total << "\n";
        cout << "Pending        : " << requests.countStatus("Pending") << "\n";
        cout << "In Progress    : " << requests.countStatus("In Progress") << "\n";
        cout << "Completed      : " << requests.countStatus("Completed") << "\n\n";
        cout << "Resource Utilization:\n";
        resources.displayUtilization();
    }

    void whatIfSimulation() const {
        int extraDoctors, extraBeds, extraXray;
        cout << "Enter hypothetical additional doctors: ";
        if (!(cin >> extraDoctors) || extraDoctors < 0) { invalidNumericInput(); return; }
        cout << "Enter hypothetical additional beds: ";
        if (!(cin >> extraBeds) || extraBeds < 0) { invalidNumericInput(); return; }
        cout << "Enter hypothetical additional X-Ray machines: ";
        if (!(cin >> extraXray) || extraXray < 0) { invalidNumericInput(); return; }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        int possibleConsultations = resources.availableCount(CONSULTATION) + extraDoctors;
        int possibleBeds = resources.availableCount(BED_ALLOCATION) + extraBeds;
        int possibleXrays = resources.availableCount(XRAY) + extraXray;
        int consultationCount = requests.countPendingService("Consultation");
        int bedCount = requests.countPendingService("Bed Allocation");
        int xrayCount = requests.countPendingService("X-Ray");

        cout << "\nHypothetical capacity check (does not change real resources):\n";
        cout << "Consultations that could be allocated: " << min(consultationCount, possibleConsultations) << "\n";
        cout << "Bed requests that could be allocated : " << min(bedCount, possibleBeds) << "\n";
        cout << "X-Ray requests that could be allocated: " << min(xrayCount, possibleXrays) << "\n";
        cout << "This is a capacity snapshot, not a time-based simulation.\n";
    }

private:
    static void invalidNumericInput() {
        cout << "Invalid input. Please enter a non-negative whole number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
};

// -------------------- INPUT HELPERS --------------------
int readInteger(const string& prompt, int minValue, int maxValue) {
    while (true) {
        cout << prompt;
        string line;
        if (!getline(cin, line)) return minValue;
        stringstream ss(line);
        int value;
        char extra;
        if ((ss >> value) && !(ss >> extra) && value >= minValue && value <= maxValue)
            return value;
        cout << "Invalid choice. Enter a number from " << minValue << " to " << maxValue << ".\n";
    }
}

string readNonEmptyLine(const string& prompt) {
    while (true) {
        cout << prompt;
        string value;
        getline(cin, value);
        if (!value.empty()) return value;
        cout << "This field cannot be empty.\n";
    }
}

// -------------------- MAIN PROGRAM --------------------
int main() {
    RequestList requests;
    ResourceManager resources;
    HistoryStack history;
    Scheduler scheduler(requests, resources, history);
    HospitalSimulation simulation(requests, resources);
    int nextRequestNumber = 101;

    while (true) {
        cout << "\n========== SMART HOSPITAL RESOURCE ALLOCATION ==========" << "\n";
        cout << "1. Add Service Request\n";
        cout << "2. Display All Requests\n";
        cout << "3. Search Request\n";
        cout << "4. Update Patient Name (Pending only)\n";
        cout << "5. Delete Request (Pending only)\n";
        cout << "6. View Priority Queues\n";
        cout << "7. Process Next Request\n";
        cout << "8. Complete Request / Release Resource\n";
        cout << "9. Display Resource Status\n";
        cout << "10. View Completed History\n";
        cout << "11. Pop Latest History Entry\n";
        cout << "12. Display Statistics\n";
        cout << "13. What-If Simulation\n";
        cout << "0. Exit\n";

        int choice = readInteger("Enter your choice: ", 0, 13);
        if (choice == 0) {
            cout << "Exiting Hospital Resource Allocation System.\n";
            break;
        }

        if (choice == 1) {
            string patient = readNonEmptyLine("Enter patient name: ");
            int serviceChoice = readInteger("Select service (1. Consultation  2. Bed Allocation  3. X-Ray): ", 1, 3);
            int categoryChoice = readInteger("Select category (1. Emergency  2. Urgent  3. Routine): ", 1, 3);
            ServiceType service = static_cast<ServiceType>(serviceChoice);
            Category category = static_cast<Category>(categoryChoice);
            string id = "R" + to_string(nextRequestNumber++);
            requests.addRequest(id, patient, serviceName(service), categoryName(category));
            scheduler.addToQueue(requests.searchRequest(id));
            cout << "Request added successfully. Request ID: " << id << "\n";
        }
        else if (choice == 2) {
            requests.displayRequests();
        }
        else if (choice == 3) {
            string id = readNonEmptyLine("Enter request ID: ");
            Request* request = requests.searchRequest(id);
            if (request == nullptr) cout << "Request not found.\n";
            else {
                cout << "ID: " << request->id << "\nPatient: " << request->patient
                     << "\nService: " << request->service << "\nCategory: " << request->category
                     << "\nStatus: " << request->status
                     << "\nAssigned Resource: " << request->assignedResource << "\n";
            }
        }
        else if (choice == 4) {
            string id = readNonEmptyLine("Enter request ID: ");
            string newName = readNonEmptyLine("Enter new patient name: ");
            if (requests.updatePatient(id, newName)) cout << "Patient name updated.\n";
            else cout << "Update failed. Request not found or it is no longer Pending.\n";
        }
        else if (choice == 5) {
            string id = readNonEmptyLine("Enter request ID to delete: ");
            Request* request = requests.searchRequest(id);
            if (request == nullptr) cout << "Request not found.\n";
            else if (request->status != "Pending") cout << "Only Pending requests can be deleted.\n";
            else if (!scheduler.removeFromQueue(id, request->category))
                cout << "Delete failed: request was not found in its priority queue.\n";
            else if (requests.removeRequest(id)) cout << "Pending request " << id << " deleted.\n";
            else cout << "Delete failed.\n";
        }
        else if (choice == 6) {
            scheduler.displayQueues();
        }
        else if (choice == 7) {
            scheduler.processNextRequest();
        }
        else if (choice == 8) {
            string id = readNonEmptyLine("Enter request ID to complete: ");
            scheduler.completeRequest(id);
        }
        else if (choice == 9) {
            resources.displayResources();
        }
        else if (choice == 10) {
            history.display();
        }
        else if (choice == 11) {
            if (!history.pop()) cout << "History is empty.\n";
        }
        else if (choice == 12) {
            simulation.showStatistics();
        }
        else if (choice == 13) {
            simulation.whatIfSimulation();
        }
    }
    return 0;
}
