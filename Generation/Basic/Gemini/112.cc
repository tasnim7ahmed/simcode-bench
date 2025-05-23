#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/radvd-helper.h"
#include "ns3/ping6-helper.h"

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixExample");

void PrintAllIpv6Addresses(Ptr<Node> node)
{
    NS_LOG_INFO("Node " << node->GetId() << " IPv6 Addresses:");
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
    if (!ipv6)
    {
        NS_LOG_WARN("No IPv6 stack on node " << node->GetId());
        return;
    }

    for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
        {
            Ipv6InterfaceAddress addr = ipv6->GetAddress(i, j);
            if (addr.IsAutoconfigured())
            {
                NS_LOG_INFO("  Interface " << i << ": " << addr.GetLocal() << " (Autoconfigured)");
            }
            else if (addr.GetLocal().IsLinkLocal())
            {
                NS_LOG_INFO("  Interface " << i << ": " << addr.GetLocal() << " (Link-local)");
            }
            else
            {
                NS_LOG_INFO("  Interface " << i << ": " << addr.GetLocal() << " (Manual)");
            }
        }
    }
}

void SetupPingAndPrintAddresses(Ptr<Node> n0, Ptr<Node> n1, Time startTime)
{
    NS_LOG_INFO("--- Setting up Ping6 and printing addresses at " << Simulator::Now().GetSeconds() << "s ---");

    PrintAllIpv6Addresses(NodeList::GetNode(0)); // Router
    PrintAllIpv6Addresses(n0);
    PrintAllIpv6Addresses(n1);

    Ptr<Ipv6> n1Ipv6 = n1->GetObject<Ipv6>();
    Ipv6Address n1AutoconfiguredAddress;
    bool found = false;
    for (uint32_t i = 0; i < n1Ipv6->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < n1Ipv6->GetNAddresses(i); ++j)
        {
            Ipv6InterfaceAddress addr = n1Ipv6->GetAddress(i, j);
            if (addr.IsAutoconfigured() && !addr.GetLocal().IsLinkLocal())
            {
                n1AutoconfiguredAddress = addr.GetLocal();
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    if (!found)
    {
        NS_LOG_ERROR("Could not find n1's autoconfigured global IPv6 address. Ping will not be set up.");
        return;
    }

    NS_LOG_INFO("n1's autoconfigured address for Ping target: " << n1AutoconfiguredAddress);

    Ping6Helper ping6Server;
    ping6Server.Set("LocalAddress", Ipv6AddressValue(Ipv6Address::GetAny()));
    ApplicationContainer serverApps = ping6Server.Install(n1);
    serverApps.Start(startTime);
    serverApps.Stop(Simulator::GetStopTime());

    Ping6Helper ping6Client;
    ping6Client.SetTarget(n1AutoconfiguredAddress);
    ping6Client.SetAttribute("Count", UintegerValue(4));
    ping6Client.SetAttribute("Interval", TimeValue(Seconds(1)));
    ping6Client.SetAttribute("Size", UintegerValue(64));

    ApplicationContainer clientApps = ping6Client.Install(n0);
    clientApps.Start(startTime + Seconds(0.5));
    clientApps.Stop(Simulator::GetStopTime());
}

int main(int argc, char *argv[])
{
    LogComponentEnable("RadvdTwoPrefixExample", LOG_LEVEL_INFO);
    LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6ListRouting", LOG_LEVEL_INFO);
    LogComponentEnable("RadvdApplication", LOG_LEVEL_ALL);
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

    Time simulationTime = Seconds(20);
    Simulator::Stop(simulationTime);

    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> R = nodes.Get(0);
    Ptr<Node> n0 = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    InternetStackHelper internetv6StackHelper;
    internetv6StackHelper.Install(nodes);

    CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaHelper.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices1;
    csmaDevices1 = csmaHelper.Create(NodeContainer(n0, R));

    NetDeviceContainer csmaDevices2;
    csmaDevices2 = csmaHelper.Create(NodeContainer(n1, R));

    Ipv6AddressHelper ipv6AddressHelper1;
    ipv6AddressHelper1.SetBase("2001:1::", "64");
    ipv6AddressHelper1.Assign(csmaDevices1.Get(1)); // Assign 2001:1::1 to R's device

    Ptr<Ipv6> R_ipv6 = R->GetObject<Ipv6>();
    int32_t iface_idx_R_n0 = R_ipv6->GetInterfaceForDevice(csmaDevices1.Get(1));
    if (iface_idx_R_n0 != -1)
    {
        R_ipv6->AddAddress(iface_idx_R_n0, Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64)));
        R_ipv6->SetForwarding(iface_idx_R_n0, true);
        NS_LOG_INFO("Assigned 2001:ABCD::1/64 to R's interface " << iface_idx_R_n0);
    }
    else
    {
        NS_LOG_ERROR("Could not get interface index for R's device connected to n0.");
        return 1;
    }

    Ipv6AddressHelper ipv6AddressHelper2;
    ipv6AddressHelper2.SetBase("2001:2::", "64");
    ipv6AddressHelper2.Assign(csmaDevices2.Get(1)); // Assign 2001:2::1 to R's device

    int32_t iface_idx_R_n1 = R_ipv6->GetInterfaceForDevice(csmaDevices2.Get(1));
    if (iface_idx_R_n1 != -1)
    {
        R_ipv6->SetForwarding(iface_idx_R_n1, true);
    }
    else
    {
        NS_LOG_ERROR("Could not get interface index for R's device connected to n1.");
        return 1;
    }

    RadvdHelper radvdHelper;
    Ptr<RadvdApplication> radvdApp = radvdHelper.Install(R).Get(0)->GetObject<RadvdApplication>();

    if (iface_idx_R_n0 != -1)
    {
        radvdApp->AddAddress(iface_idx_R_n0, Ipv6Address("2001:1::"), Ipv6Prefix(64));
        radvdApp->AddAddress(iface_idx_R_n0, Ipv6Address("2001:ABCD::"), Ipv6Prefix(64));
        NS_LOG_INFO("Radvd configured for interface " << iface_idx_R_n0 << " with prefixes 2001:1::/64 and 2001:ABCD::/64");
    }

    if (iface_idx_R_n1 != -1)
    {
        radvdApp->AddAddress(iface_idx_R_n1, Ipv6Address("2001:2::"), Ipv6Prefix(64));
        NS_LOG_INFO("Radvd configured for interface " << iface_idx_R_n1 << " with prefix 2001:2::/64");
    }

    radvdApp->Start(Seconds(0.1));
    radvdApp->Stop(simulationTime);

    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Schedule(Seconds(5.0), &SetupPingAndPrintAddresses, n0, n1, Seconds(5.5));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("radvd-two-prefix.tr");
    csmaHelper.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}