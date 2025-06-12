#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create Helper objects
    LteHelper lteHelper;
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Setup eNB
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper.InstallEnbDevice(enbNodes);

    // Setup UE
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;

    // Attach the UE to the eNB
    lteHelper.Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Install the IP stack on the eNB
    internet.Install(enbNodes);
    Ipv4InterfaceContainer enbIpIface;

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = ip.Assign(ueDevs);
    enbIpIface = ip.Assign(enbDevs);

    // Setup TCP Application (UE -> eNB)
    uint16_t dlPort = 20000;
    Address dlSinkAddress(InetSocketAddress(enbIpIface.GetAddress(0), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer dlSinkApp = dlPacketSinkHelper.Install(enbNodes.Get(0));
    dlSinkApp.Start(Seconds(1.0));
    dlSinkApp.Stop(Seconds(10.0));

    // Setup OnOff Application for TCP Data Transfer
    OnOffHelper dlClientHelper("ns3::TcpSocketFactory", dlSinkAddress);
    dlClientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    dlClientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    dlClientHelper.SetAttribute("DataRate", DataRateValue(DataRate("10Mb/s")));
    dlClientHelper.SetAttribute("PacketSize", UintegerValue(1500));
    ApplicationContainer dlClientApp = dlClientHelper.Install(ueNodes.Get(0));
    dlClientApp.Start(Seconds(2.0));
    dlClientApp.Stop(Seconds(10.0));

    // Activate tracing
    Ptr<RadioBearerStatsCalculator> rbs = lteHelper.GetRadioBearerStatsCalculator(ueNodes.Get(0), 1);
    rbs->SetAttribute ("EpochDuration", TimeValue (Seconds (0.1)));

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}