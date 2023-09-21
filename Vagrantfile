
Vagrant.configure("2") do |config|
    config.vm.box = "bento/ubuntu-22.04"
    config.vm.provider :virtualbox do |v|
      v.name = "bazel@tw"
      v.gui = false
      v.memory = 2048
    end
  
    if Vagrant.has_plugin?("vagrant-vbguest")
      config.vbguest.auto_update = false  
    end
  
    config.vm.synced_folder ".", "/vagrant", type: "virtualbox"
  
    # Update repositories
    config.vm.provision :shell, inline: "sudo apt update -y"
  
    # Upgrade installed packages
    config.vm.provision :shell, inline: "sudo apt upgrade -y"
  
    # Install CMake
    config.vm.provision :shell, inline: "sudo snap install cmake --classic"
  
    # Install Vsomeip dependencies
    config.vm.provision :shell, inline: "sudo apt-get install build-essential -y"
    config.vm.provision :shell, inline: "sudo apt install libboost-filesystem-dev libboost-system-dev libboost-thread-dev libboost-program-options-dev libboost-test-dev -y"
      
    # Install Bazel
    config.vm.provision :shell, inline: "sudo apt install curl gnupg -y"
    config.vm.provision :shell, inline: "curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor > bazel.gpg"
    config.vm.provision :shell, inline: "sudo mv bazel.gpg /etc/apt/trusted.gpg.d/"
    config.vm.provision :shell, inline: "echo 'deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8' | sudo tee /etc/apt/sources.list.d/bazel.list"
    config.vm.provision :shell, inline: "sudo apt update && sudo apt install bazel -y"
  
    # Add `vagrant` to Administrator
    config.vm.provision :shell, inline: "sudo usermod -a -G sudo vagrant"
  
    # Restart
    config.vm.provision :shell, inline: "sudo shutdown -r now"
  end