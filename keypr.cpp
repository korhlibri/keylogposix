#include <fcntl.h>
#include <linux/input.h>
#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <thread>
#include <filesystem>
#include <signal.h>
#include <poll.h>

std::vector<std::string> event_files;
std::vector<pollfd> file_descriptors;

// Exits all the files gracefully before shutting down.
void exit_handler(int s) {
    for (pollfd poll_fd : file_descriptors) {
        close(poll_fd.fd);
    }
    exit(0);
}

int main() {
    std::string path = "/dev/input/";

    // Gets all the event files from the /dev/input/ directory.
    // Event files contain input events (keyboard and mouse keys).
    for (const auto & entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().string().substr(11, 5) == "event") {
            event_files.push_back(entry.path().string());
        }
    }

    // Every event file will be opened and saved to a pollfd struct.
    // Pollfd struct is needed for the poll command to poll all the files for changes.
    // 
    // First element in struct is the file descriptor.
    // Second element in struct are the events that we want to poll for. POLLIN means input events.
    // Third element in struct is returned by the poll function that contains the events that are 
    // available.
    for (const std::string & event_file : event_files) {
        const char *c_file = event_file.c_str();
        int fd = open(c_file, O_RDONLY);
        if (fd >= 0) {
            pollfd pfd = { fd, POLLIN };
            file_descriptors.push_back(pfd);
        }
    }
    // Gets a pointer to the first element in a vector to act as an array, as well as size to avoid
    // out of bounds errors.
    // Poll takes an array of pollfd structs to poll all of them for events.
    pollfd* poll_fd = &file_descriptors[0];
    int poll_fd_len = static_cast<int>(file_descriptors.size());

    // Input events will be saved here upon reading.
    input_event ev;

    // This signal handles interrupts from Ctrl+C and reroutes it to a graceful exit.
    signal(SIGINT, exit_handler);

    while (true) {
        // Poll command polls the events to see if there is a wanted event available.
        // The 20 means a 20 milisecond timeout on event polling.
        // 
        // Reading without polling does not work as read() hangs until there is something to read.
        // Polling allows us to wait for information to be available before reading.
        int n = poll(poll_fd, poll_fd_len, 20);
        for (int i = 0; i < poll_fd_len; i++) {
            // If a return event in revents is an input event, we can then read it and use it.
            // In this case, the program prints out the event to the console.
            if (poll_fd[i].revents & POLLIN) {
                read(poll_fd[i].fd, &ev, sizeof(ev));
                if (ev.type == 1 && ev.value != 2) {
                    // Do note that keys have to be converted from key maps to characters if needed.
                    // This varies by keyboard. Generally, 1 is Escape key, 2 is 1 key, etc...
                    // "dumpkeys" or "xmodpak" commands can be used to get the key maps of chars.
                    std::cout << poll_fd[i].fd << " " << ev.code << " " << ev.value << std::endl;
                }
            }
        }
        // Arbitrary sleep to not use up many resources.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}