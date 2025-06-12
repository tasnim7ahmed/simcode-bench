#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimpleTcpExample");

int
main(int argc, char* argv[])
{
    // Enable logging
    LogComponentEnable("LteSimpleTcpExample", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;

    enbNodes.Create(1);
    ueNodes.Create(1);

    // Create the LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create and set the EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set path loss model
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UE and eNodeB
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to the eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up TCP server and client applications
    uint16_t dlPort = 1234;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(enbNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Setup OnOff application for TCP traffic
    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(ueIpIface.GetAddress(0), dlPort)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = onoff.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}