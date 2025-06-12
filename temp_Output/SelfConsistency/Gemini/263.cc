#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSingleCellRandomWalk");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LteSingleCellRandomWalk", LOG_LEVEL_INFO);

    // Set the simulation time
    double simTime = 10.0;

    // Set the number of UEs
    uint16_t numUes = 1;

    // Set the UDP echo application parameters
    uint16_t dlPort = 9;
    uint32_t packetSize = 512;
    uint32_t numPackets = 10;

    // Create the eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Set the eNodeB transmitter properties
    lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));

    // Create the eNodeB and UE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);

    // Install the Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses to the nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

    // Attach the UE to the eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Activate a data radio bearer between UE and eNodeB
    enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
    EpsBearer bearer(q);
    lteHelper->ActivateDataRadioBearer(ueDevs, bearer);

    // Create UDP echo server application on the UE
    UdpEchoServerHelper echoServer(dlPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(simTime - 0.5));

    // Create UDP echo client application on the eNodeB
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), dlPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Enable PCAP tracing
    // lteHelper->EnableTraces();

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();
    return 0;
}