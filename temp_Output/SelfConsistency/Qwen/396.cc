#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpClientServerSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for UdpServer and UdpClient
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);

    // Create nodes: one for eNB, one for UE
    NodeContainer enbNode;
    enbNode.Create(1);

    NodeContainer ueNode;
    ueNode.Create(1);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNode);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNode);

    // Attach UE to eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install the IP stack
    InternetStackHelper internet;
    internet.Install(ueNode);
    internet.Install(enbNode);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    Ipv4Address serverIp = ueIpIface.GetAddress(0, 1); // UE's IP address

    // Set up applications
    uint16_t port = 12345;

    // Server (on UE)
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNode.Get(0));
    serverApp.Start(Seconds(0.01));
    serverApp.Stop(Seconds(20.0));

    // Client (on eNB)
    UdpClientHelper client(serverIp, port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(enbNode.Get(0));
    clientApp.Start(Seconds(0.02));
    clientApp.Stop(Seconds(20.0));

    // Enable LTE logging
    lteHelper->EnableLogComponents();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}