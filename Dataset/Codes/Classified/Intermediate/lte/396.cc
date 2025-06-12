#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteExample");

int main(int argc, char *argv[])
{
    // Set the simulation time
    Time::SetResolution(Time::MS);

    // Create nodes
    NodeContainer eNBNodes;
    eNBNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create EPC helper
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(eNBNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(ueNodes);
    stack.Install(eNBNodes);

    // Assign IP addresses to the nodes
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(ueDevs);

    // Attach UE to the eNB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set up a UDP server on UE
    uint16_t port = 12345;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up a UDP client on eNB
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(eNBNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
