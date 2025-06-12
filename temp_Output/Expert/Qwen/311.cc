#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint16_t numberOfUes = 2;
    Time serverStartTime = Seconds(1.0);
    Time clientStartTime = Seconds(2.0);
    Time simDuration = Seconds(10.0);
    uint32_t packetSize = 1024;
    uint32_t totalBytes = 1000000;

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRlc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4H;
    ipv4H.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIfaces = ipv4H.Assign(ueDevs);

    uint16_t dlPort = 8080;

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer sinkApp = sink.Install(ueNodes.Get(0));
    sinkApp.Start(serverStartTime);
    sinkApp.Stop(simDuration);

    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort));
    client.SetAttribute("MaxBytes", UintegerValue(totalBytes));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1000000bps")));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(ueNodes.Get(1));
    clientApp.Start(clientStartTime);
    clientApp.Stop(simDuration);

    Simulator::Stop(simDuration);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}