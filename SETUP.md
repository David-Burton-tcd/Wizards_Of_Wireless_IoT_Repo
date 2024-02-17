# Setup Development Environment

## Download ESP - IDF
This section documents installation of the offficial development framework for ESP32 device family.

- There are two approaches here - VSCode or Eclipse.
- Go to this [link](https://dl.espressif.com/dl/esp-idf/) and IDF SDK (just library, 3rd option) or IDF SDK + IDE (library + modified Eclipse IDE, 2nd Option).
- If you are gonna use VSC, download 3rd option

## Setup ESP IDE
- If ESP IDE is installed, open the application and select `Espresssif > Download and configure ESP-IDF` from menu bar.
- Check "Use existing ESP-IDF directory" and then give path `C:\Espressif\frameworks\esp-idf-<your-version>` and click Finish.
- Then select `ESP-IDF Tools Manager > Install Tools` from menu bar and click `Install Tools`.
- You will now be able to create IDF projects in the IDE.

## Setup hello-world project
Follow steps on this link:
https://github.com/espressif/idf-eclipse-plugin#create-a-new-project
