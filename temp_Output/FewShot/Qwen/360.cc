#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseCa", BooleanValue(false));

    // Create eNB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    NodeContainer allNodes = NodeContainer(enbNodes, ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to an eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up UDP Echo Server on the eNB
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on the UE
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(80));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}