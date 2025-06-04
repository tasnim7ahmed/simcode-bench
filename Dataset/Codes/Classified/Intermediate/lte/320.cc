#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleLteExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleLteExample", LOG_LEVEL_INFO);

    // Create nodes: one eNodeB (base station) and multiple UEs
    NodeContainer enbNode, ueNodes;
    enbNode.Create(1);
    ueNodes.Create(3);

    // Set up LTE helper and install LTE stack
    LteHelper lteHelper;
    lteHelper.SetSchedulerType("ns3::PfSocketScheduler");
    NetDeviceContainer enbDevice, ueDevices;
    enbDevice = lteHelper.InstallEnbDevice(enbNode);
    ueDevices = lteHelper.InstallUeDevice(ueNodes);

    // Install internet stack (IP, UDP, etc.)
    InternetStackHelper stack;
    stack.Install(enbNode);
    stack.Install(ueNodes);

    // Assign IP addresses to UEs and eNodeB
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevices);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevice);

    // Set up UDP server on the eNodeB
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(enbNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on each UE
    UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(ueNodes);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Attach UEs to the eNodeB
    lteHelper.Attach(ueDevices.Get(0), enbDevice.Get(0));
    lteHelper.Attach(ueDevices.Get(1), enbDevice.Get(0));
    lteHelper.Attach(ueDevices.Get(2), enbDevice.Get(0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
