#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP applications
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Create EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create nodes: 1 eNB and 1 UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;

    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the first eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up UDP Server on UE (node 1)
    uint16_t port = 12345;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(0.1));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP Client on eNB (node 0)
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(enbNodes.Get(0));
    clientApp.Start(Seconds(0.2));
    clientApp.Stop(Seconds(20.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}