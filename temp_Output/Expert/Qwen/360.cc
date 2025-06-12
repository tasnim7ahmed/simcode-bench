#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue(false));
    Config::SetDefault("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue(false));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = ipv4h.Assign(ueDevs);
    Ipv4InterfaceContainer enbIpIface;
    enbIpIface = ipv4h.Assign(enbDevs);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    uint16_t dlPort = 1234;
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer dlServerApps = dlServer.Install(enbNodes.Get(0));
    dlServerApps.Start(Seconds(1.0));
    dlServerApps.Stop(Seconds(10.0));

    UdpClientHelper dlClient(enbIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer dlClientApps = dlClient.Install(ueNodes.Get(0));
    dlClientApps.Start(Seconds(2.0));
    dlClientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}