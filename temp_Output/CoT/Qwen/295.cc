#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for LTE modules if needed
    // LogComponentEnable("LteHelper", LOG_LEVEL_INFO);

    double simDuration = 10.0; // seconds

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;

    enbNodes.Create(1);
    ueNodes.Create(1);

    // Setup mobility: eNodeB is stationary, UE has random mobility
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(5.0));
    mobilityUe.Install(ueNodes);

    // Create LTE helper and configure it
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseBuildings", BooleanValue(false));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the first eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Setup applications
    uint16_t dlPort = 12345;

    // UDP server (on UE)
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer dlServerApps = dlServer.Install(ueNodes.Get(0));
    dlServerApps.Start(Seconds(0.0));
    dlServerApps.Stop(Seconds(simDuration));

    // UDP client (on eNodeB)
    UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer dlClientApps = dlClient.Install(enbNodes.Get(0));
    dlClientApps.Start(Seconds(0.1));
    dlClientApps.Stop(Seconds(simDuration));

    // Set simulation time and run
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}