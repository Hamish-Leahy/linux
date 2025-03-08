// SPDX-License-Identifier: GPL-2.0-only
/*
 * IRQ offload/bypass manager
 *
 * Copyright (C) 2015 Red Hat, Inc.
 * Copyright (c) 2015 Linaro Ltd.
 *
 * Various virtualization hardware acceleration techniques allow bypassing or
 * offloading interrupts received from devices around the host kernel.  Posted
 * Interrupts on Intel VT-d systems can allow interrupts to be received
 * directly by a virtual machine.  ARM IRQ Forwarding allows forwarded physical
 * interrupts to be directly deactivated by the guest.  This manager allows
 * interrupt producers and consumers to find each other to enable this sort of
 * bypass.
 */

#include <linux/irqbypass.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IRQ bypass manager utility module");

static LIST_HEAD(producers);
static LIST_HEAD(consumers);
static DEFINE_MUTEX(lock);

/* Connect a producer and a consumer */
static int __connect(struct irq_bypass_producer *prod,
		     struct irq_bypass_consumer *cons)
{
	int ret = 0;

	if (prod->stop)
		prod->stop(prod);
	if (cons->stop)
		cons->stop(cons);

	if (prod->add_consumer) {
		ret = prod->add_consumer(prod, cons);
		if (ret)
			return ret;
	}

	ret = cons->add_producer(cons, prod);
	if (ret && prod->del_consumer)
		prod->del_consumer(prod, cons);

	if (cons->start)
		cons->start(cons);
	if (prod->start)
		prod->start(prod);

	return ret;
}

/* Disconnect a producer and a consumer */
static void __disconnect(struct irq_bypass_producer *prod,
			 struct irq_bypass_consumer *cons)
{
	if (prod->stop)
		prod->stop(prod);
	if (cons->stop)
		cons->stop(cons);

	cons->del_producer(cons, prod);
	if (prod->del_consumer)
		prod->del_consumer(prod, cons);

	if (cons->start)
		cons->start(cons);
	if (prod->start)
		prod->start(prod);
}

/**
 * irq_bypass_register_producer - Register an IRQ bypass producer
 * @producer: Pointer to producer structure
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int irq_bypass_register_producer(struct irq_bypass_producer *producer)
{
	struct irq_bypass_producer *tmp;
	struct irq_bypass_consumer *consumer;
	int ret;

	if (!producer->token)
		return -EINVAL;

	might_sleep();

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	mutex_lock(&lock);

	list_for_each_entry(tmp, &producers, node) {
		if (tmp->token == producer->token) {
			ret = -EBUSY;
			goto out_err;
		}
	}

	list_for_each_entry(consumer, &consumers, node) {
		if (consumer->token == producer->token) {
			ret = __connect(producer, consumer);
			if (ret)
				goto out_err;
			break;
		}
	}

	list_add(&producer->node, &producers);
	mutex_unlock(&lock);
	return 0;

out_err:
	mutex_unlock(&lock);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL_GPL(irq_bypass_register_producer);

/**
 * irq_bypass_unregister_producer - Unregister an IRQ bypass producer
 * @producer: Pointer to producer structure
 *
 * Returns nothing.
 */
void irq_bypass_unregister_producer(struct irq_bypass_producer *producer)
{
	struct irq_bypass_producer *tmp;
	struct irq_bypass_consumer *consumer;

	if (!producer->token)
		return;

	might_sleep();

	if (!try_module_get(THIS_MODULE))
		return; /* nothing in the list anyway */

	mutex_lock(&lock);

	list_for_each_entry(tmp, &producers, node) {
		if (tmp->token != producer->token)
			continue;

		list_for_each_entry(consumer, &consumers, node) {
			if (consumer->token == producer->token) {
				__disconnect(producer, consumer);
				break;
			}
		}

		list_del(&producer->node);
		module_put(THIS_MODULE);
		break;
	}

	mutex_unlock(&lock);
	module_put(THIS_MODULE);
}
EXPORT_SYMBOL_GPL(irq_bypass_unregister_producer);

/**
 * irq_bypass_register_consumer - Register an IRQ bypass consumer
 * @consumer: Pointer to consumer structure
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int irq_bypass_register_consumer(struct irq_bypass_consumer *consumer)
{
	struct irq_bypass_consumer *tmp;
	struct irq_bypass_producer *producer;
	int ret;

	if (!consumer->token || !consumer->add_producer || !consumer->del_producer)
		return -EINVAL;

	might_sleep();

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	mutex_lock(&lock);

	list_for_each_entry(tmp, &consumers, node) {
		if (tmp->token == consumer->token || tmp == consumer) {
			ret = -EBUSY;
			goto out_err;
		}
	}

	list_for_each_entry(producer, &producers, node) {
		if (producer->token == consumer->token) {
			ret = __connect(producer, consumer);
			if (ret)
				goto out_err;
			break;
		}
	}

	list_add(&consumer->node, &consumers);
	mutex_unlock(&lock);
	return 0;

out_err:
	mutex_unlock(&lock);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL_GPL(irq_bypass_register_consumer);

/**
 * irq_bypass_unregister_consumer - Unregister an IRQ bypass consumer
 * @consumer: Pointer to consumer structure
 *
 * Returns nothing.
 */
void irq_bypass_unregister_consumer(struct irq_bypass_consumer *consumer)
{
	struct irq_bypass_consumer *tmp;
	struct irq_bypass_producer *producer;

	if (!consumer->token)
		return;

	might_sleep();

	if (!try_module_get(THIS_MODULE))
		return; /* nothing in the list anyway */

	mutex_lock(&lock);

	list_for_each_entry(tmp, &consumers, node) {
		if (tmp != consumer)
			continue;

		list_for_each_entry(producer, &producers, node) {
			if (producer->token == consumer->token) {
				__disconnect(producer, consumer);
				break;
			}
		}

		list_del(&consumer->node);
		module_put(THIS_MODULE);
		break;
	}

	mutex_unlock(&lock);
	module_put(THIS_MODULE);
}
EXPORT_SYMBOL_GPL(irq_bypass_unregister_consumer);
