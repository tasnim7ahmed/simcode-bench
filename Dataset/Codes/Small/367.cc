#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpExample");

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer ueNode;
    NodeContainer enbNode;
    ueNode.Create(1);
    enbNode.Create(1);

    // Install LTE stack
    LteHelper lte;
    InternetStackHelper internet;
    internet.Install(ueNode);
    internet.Install(enbNode);

    // Set up the eNodeB and UE devices
    NetDeviceContainer ueDevice;
    NetDeviceContainer enbDevice;
    enbDevice = lte.InstallEnbDevice(enbNode);
    ueDevice = lte.InstallUeDevice(ueNode);

    // Set up IP addressing and assign IP to UE
    Ipv4InterfaceContainer ueIpInterface;
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.1.0", "255.255.255.0");
    ueIpInterface = ipv4h.Assign(ueDevice);

    // Attach UE to eNodeB
    lte.Attach(ueDevice, enbDevice.Get(0));

    // Set up TCP application on UE (client)
    uint16_t port = 9;
    TcpClientHelper tcpClient(ueIpInterface.GetAddress(0), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = tcpClient.Install(enbNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up TCP server application on eNodeB (server)
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(ueNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
